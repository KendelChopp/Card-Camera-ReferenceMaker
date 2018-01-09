
#include "opencv2/core/utility.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include <stdio.h>
#include <string>
#include <iostream>
using namespace cv;
using namespace std;
int edgeThresh = 1;
static int CARD_MIN_AREA = 6000;
static int CORNER_WIDTH = 56;
static int CORNER_HEIGHT = 160;
static int SUIT_HEIGHT = 50;
static int CARD_WIDTH = 400;
static int CARD_HEIGHT = 600;
static int MAX_BOUNDS = 42;
Mat image, gray, edge, cedge;
const char* keys =
{
    "{help h||}{@image |../data/fruits.jpg|input image name}"
};
bool compareContourAreas ( std::vector<Point> contour1, std::vector<Point> contour2 ) {
     double i = fabs( contourArea(Mat(contour1)) );
     double j = fabs( contourArea(Mat(contour2)) );
     return ( i > j );
}

//Blur and threshold the input
Mat preprocessVideo(Mat input)
{
    Mat modify;
    cvtColor(input, modify, CV_BGR2GRAY);
  
    GaussianBlur(modify, modify, cvSize(5,5),0);
    threshold(modify, modify, 120, 255, CV_THRESH_BINARY);
    
    return modify;
}


//Warp the card into an acceptable format
Mat warpCard(Mat originalImage, Point2f points[4], int width, int height)
{
    Point2f src_vertices[4];
    /*
     * pts[0] = RED bl
     * pts[1] = GREEN tl
     * pts[2] = BLUE tr
     * pts[3] = WHITE br
     */
    Point center = Point((points[0].x + points[2].x) / 2, (points[0].y + points[2].y) / 2);
    if (width <= 0.8*height) {
        //broken
        if (points[0].x < center.x) {
            //BL->BR->TR->TL
            src_vertices[0] = points[3];
            src_vertices[1] = points[0];
            src_vertices[2] = points[1];
            src_vertices[3] = points[2];
        } else {
            //BL->TL->TR->BR
            src_vertices[0] = points[0];
            src_vertices[1] = points[1];
            src_vertices[2] = points[2];
            src_vertices[3] = points[3];
        }
    } else if (width >= 1.2*height) {
        //BR->BL->TL->TR
        if (points[0].x < center.x) {
            src_vertices[0] = points[2];
            src_vertices[1] = points[3];
            src_vertices[2] = points[0];
            src_vertices[3] = points[1];
        } else {
            src_vertices[0] = points[1];
            src_vertices[1] = points[2];
            src_vertices[2] = points[3];
            src_vertices[3] = points[0];
        }
    } else {//tl, bl, br, tr//bl,br,tr,tl
        //RETURN TO THIS CASE LATER
        if (points[0].x > points[2].x) {
            src_vertices[0] = points[2];
            src_vertices[1] = points[3];
            src_vertices[2] = points[0];
            src_vertices[3] = points[1];
        } else {
            src_vertices[0] = points[1];
            src_vertices[1] = points[2];
            src_vertices[2] = points[3];
            src_vertices[3] = points[0];
        }
        
    }
   // cout << points[0] << "," << points[1] << points[2] << "," << points[3] << "\n";
    //BL->TL->TR->BR
    Point2f dst_vertices[4];
    dst_vertices[0] = Point(0, 0);
    dst_vertices[1] = Point(0, CARD_WIDTH-1);
    dst_vertices[2] = Point(CARD_HEIGHT-1, CARD_WIDTH-1);
    dst_vertices[3] = Point(CARD_HEIGHT-1, 0);
    Mat warpPerspectiveMatrix = getPerspectiveTransform(src_vertices, dst_vertices);
    Mat card;
    warpPerspective(originalImage, card, warpPerspectiveMatrix, Size(CARD_HEIGHT,CARD_WIDTH));
    cvtColor(card, card, CV_BGR2GRAY);
    return card;
}

//Warp the card then crop and threshold to the corner
Mat preprocessCardCorner(Mat contour, Mat sourceImage)
{
    double perimeter = arcLength(contour, true);
    Mat approx;
    approxPolyDP(contour, approx, 0.01*perimeter, true);
    Rect rect;
    rect = boundingRect(contour);
    Point2f pts[4];
    pts[0] = Point(approx.at<int>(0, 0), approx.at<int>(0, 1));
    pts[1] = Point(approx.at<int>(1, 0), approx.at<int>(1, 1));
    pts[2] = Point(approx.at<int>(2, 0), approx.at<int>(2, 1));
    pts[3] = Point(approx.at<int>(3, 0), approx.at<int>(3, 1));
    //approx.points(pts);
    Mat warpedCard = warpCard(sourceImage, pts, rect.width, rect.height);
    //return warpedCard;

    Rect crop = Rect(0,warpedCard.rows - 1 - CORNER_WIDTH, CORNER_HEIGHT, CORNER_WIDTH);
    
    Mat corner;
    //Mat cornerZoom;
    warpedCard(crop).copyTo(corner);
    //return corner;
    //resize(corner, cornerZoom, Size(4 * corner.cols,4 * corner.rows));
     
 
    threshold(corner, corner, 120, 255, CV_THRESH_BINARY_INV);

    return corner;
}

//Find the contours
vector<Mat> findCardContours(Mat preprocessedImage)
{
    vector<Mat>  contours;
    //vector<vector<Point>>  hierarchy;
    findContours(preprocessedImage, contours, RETR_LIST, CHAIN_APPROX_SIMPLE);
    
    std::sort(contours.begin(), contours.end(), compareContourAreas);
    vector<Mat>  cards;
    for (int i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        if (area < CARD_MIN_AREA) break;
        
        double arc = arcLength(contours[i], true);
        Mat approx;
        approxPolyDP(contours[i], approx, 0.01*arc, true);
        if (approx.rows == 4) {
            cards.push_back(contours[i]);
        }
    }
    return cards;
}

//Bound the corner to the white points
Rect getCornerBounded(Mat corner) {
    Mat points;
    findNonZero(corner, points);
    Rect minRect;
    minRect = boundingRect(points);
    return minRect;
}

//Write the new references images
void getReferenceImages(Mat input)
{

    Mat modifiedImage;
    modifiedImage = preprocessVideo(input);

    vector<Mat> cards = findCardContours(modifiedImage);
    if (cards.size() < 1) {
        return;
    }

    Mat card = preprocessCardCorner(cards[0], input);
    Rect bounds = getCornerBounded(card);


    if (bounds.height > MAX_BOUNDS) {
        Rect newFrame = Rect(0,bounds.height - MAX_BOUNDS - 1 , card.cols, card.rows - (bounds.height - MAX_BOUNDS - 1));
        card(newFrame).copyTo(card);
        bounds = getCornerBounded(card);
    }

    Mat finalCorner;
    card(bounds).copyTo(finalCorner);
    
    Mat suitArea;
    Rect suitFrame = Rect(finalCorner.cols - SUIT_HEIGHT,0, SUIT_HEIGHT, finalCorner.rows);
    finalCorner(suitFrame).copyTo(suitArea);
   
    
    Mat suitNonZero;
    findNonZero(suitArea, suitNonZero);
    
    suitArea(boundingRect(suitNonZero)).copyTo(suitArea);
    
    resize(suitArea,suitArea, Size(38,34));

    Mat numberArea;
    Rect numberFrame = Rect(0,0,finalCorner.cols - SUIT_HEIGHT - 2, finalCorner.rows);
    finalCorner(numberFrame).copyTo(numberArea);
    Mat numberNonZero;
    findNonZero(numberArea, numberNonZero);
    numberArea(boundingRect(numberNonZero)).copyTo(numberArea);
    


	Mat finalNumber;
	resize(numberArea, finalNumber, Size(62,34));
    
    imwrite("modified_images/numberReferencePage.png", finalNumber);
    imwrite("modified_images/suitReferencePage.png", suitArea);
}

// Run with ./test <image file>
int main( int argc, const char** argv )
{
    CommandLineParser parser(argc, argv, keys);
    
    string filename = parser.get<string>(0);
    image = imread(filename, 1);
    if(image.empty())
    {
        printf("Cannot read image file: %s\n", filename.c_str());
        return -1;
    }
    Mat newImage;
    getReferenceImages(image);

    return 0;
}