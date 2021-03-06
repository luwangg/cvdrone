#include "ardrone/ardrone.h"
#include <opencv2/stitching/stitcher.hpp>

// --------------------------------------------------------------------------
// main(Number of arguments, Argument values)
// Description  : This is the entry point of the program.
// Return value : SUCCESS:0  ERROR:-1
// --------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    // AR.Drone class
    ARDrone ardrone;

    // Initialize
    if (!ardrone.open()) {
        std::cout << "Failed to initialize." << std::endl;
        return -1;
    }

    // Snapshots
    std::vector<cv::Mat> snapshots;

    // Key frame
    cv::Mat last = ardrone.getImage();

    // ORB detector/descriptor
    cv::OrbFeatureDetector detector;
    cv::OrbDescriptorExtractor extractor;

    // Main loop
    while (1) {
        // Key input
        int key = cv::waitKey(1);
        if (key == 0x1b) break;

        // Get an image
        cv::Mat image = ardrone.getImage();

        // Detect key points
        cv::Mat descriptorsA, descriptorsB;
        std::vector<cv::KeyPoint> keypointsA, keypointsB;
        detector.detect(last, keypointsA);
        detector.detect(image, keypointsB);
        extractor.compute(last, keypointsA, descriptorsA);
        extractor.compute(image, keypointsB, descriptorsB);

        // Match key points
        std::vector<cv::DMatch> matches;
        cv::BFMatcher matcher(cv::NORM_HAMMING, true);
        matcher.match(descriptorsA, descriptorsB, matches);

        // Scene change flag
        int scene_changed = 0;

        // Inliers
        std::vector<cv::DMatch> inliers;

        // Get enough matches
        if (matches.size() > 100) {
            // Prepare for cv::findHomography
            std::vector<cv::Point2f> srcPoints, dstPoints;
            for (size_t i = 0; i < matches.size(); i++) {
                srcPoints.push_back(keypointsA[matches[i].queryIdx].pt);
                dstPoints.push_back(keypointsB[matches[i].trainIdx].pt);
            }

            // Calculate homography matrix
            std::vector<uchar> status;
            cv::Mat H = cv::findHomography(srcPoints, dstPoints, status, cv::RANSAC);

            // Save good matches
            for (size_t i = 0; i < matches.size(); i++) {
                if (status[i]) inliers.push_back(matches[i]);
            }

            // Resulting less inliers means the scene is changed
            if (inliers.size() < 80) scene_changed = 1;
        }
        // Having less matches means the scene is changed
        else scene_changed = 1;

        // Take a snapshot when scene was changed
        if (scene_changed) {
            image.copyTo(last);
            cv::Ptr<cv::Mat> tmp(new cv::Mat());
            image.copyTo(*tmp);
            snapshots.push_back(*tmp);
        }

        // Display the image
        cv::Mat matchImage;
        cv::drawMatches(last, keypointsA, image, keypointsB, inliers, matchImage, cv::Scalar::all(-1), cv::Scalar::all(-1), std::vector<char>(), cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);
        cv::imshow("camera", matchImage);
    }

    // Destroy window(s)
    cv::destroyAllWindows();

    // Stiching
    cv::Mat result;
    cv::Stitcher stitcher = cv::Stitcher::createDefault();
    std::cout << "Stitching images... (this takes a long time)" << std::endl;
    if (stitcher.stitch(snapshots, result) == cv::Stitcher::OK) {
        cv::imshow("result", result);
        cv::imwrite("result.jpg", result);
        cv::waitKey(0);
    }

    // See you
    ardrone.close();

    return 0;
}