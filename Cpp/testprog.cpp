#include <opencv2/core/core.hpp>
#include <iostream>
#include <stdio.h>

//Mine
#include "convertAhandaPovRayToStandard.h"
#include "CostVolume/utils/reproject.hpp"
#include "CostVolume/utils/reprojectCloud.hpp"
#include "CostVolume/Cost.h"
#include "CostVolume/CostVolume.hpp"
#include "Optimizer/Optimizer.hpp"
#include "DepthmapDenoiseWeightedHuber/DepthmapDenoiseWeightedHuber.hpp"
// #include "OpenDTAM.hpp"
#include "graphics.hpp"
#include "set_affinity.h"
#include "Track/Track.hpp"
#include "apriltag/apriltag.h"
#include "apriltag/apriltag_pose.h"
#include "apriltag/tagStandard41h12.h"

#include "utils/utils.hpp"

//debug
#include "tictoc.h"

const static bool valgrind=0;

//A test program to make the mapper run
using namespace cv;
using namespace cv::cuda;
using namespace std;

int App_main( int argc, char** argv );

void myExit(){
    ImplThread::stopAllThreads();
}
int main( int argc, char** argv ){

    initGui();

    int ret=App_main(argc, argv);
    myExit();
    return ret;
}

int App_main( int argc, char** argv )
{
    int numImg=10;

#if !defined WIN32 && !defined _WIN32 && !defined WINCE && defined __linux__ && !defined ANDROID
    pthread_setname_np(pthread_self(),"App_main");
#endif

    char filename[500];
    Mat image, cameraMatrix, R, T;
    vector<Mat> images,Rs,Ts,Rs0,Ts0;
    Mat ret;//a place to return downloaded images to

    
    double reconstructionScale=5/5.;
	
	apriltag_detector_t *td = apriltag_detector_create();
	apriltag_family_t *tf = tagStandard41h12_create();
	apriltag_detector_add_family(td, tf);

    for(int i=0;i<numImg;i++){
        Mat tmp;
        sprintf(filename,"%s/scene_%03d.jpg",argv[1],i);
//        sprintf(filename,"../../Trajectory_30_seconds/scene_%03d.png",i);
        //convertAhandaPovRayToStandard(argv[1],
        //                              i,
        //                              cameraMatrix,
        //                              R,
        //                              T);
		// Try to detect an April tag
		
		// From our opencv undistort
		cameraMatrix.data = {392.31866455, 0, 314.93378122,
							 0, 522.89459229, 251.50994903,
							 0, 0, 1};
        Mat image;
        cout<<"Opening: "<< filename << endl;
        
        imread(filename, -1).convertTo(image,CV_32FC3,1.0/65535.0);
        resize(image,image,Size(),reconstructionScale,reconstructionScale);
        
		img_u8_t* byte_image = image_u8_create_from_pnm(filename); 
		zarray_t *detections = apriltag_detector_detect(td, byte_image);
		
		Mat R = Mat(3, 3, CV_64FC1);
		Mat t = Mat(3, 1, CV_64FC1);
		if (zarray_size(detections) > 0) {
			for (int i = 0; i < 1; i++) {
		    	apriltag_detection_t *det;
			    zarray_get(detections, i, &det);

		    // Do stuff with detections here.
				apriltag_detection_info_t info;
				info.det = det;
				info.tagsize = 0.02; //TODO: Change as needed
				info.fx = 392.31866455;
				info.fy = 522.89459229;
				info.cx = 314.93378122;
				info.cy = 251.50994903;
				apriltag_pose_t pose;
				estimate_tag_pose(&info, &pose);
			}	
			matd_t *pR = pose.R;
			R = Mat(2, {R->nrows, R->ncols}, CV_64FC1, pR->data);
			matd_t *pt = pose.t;
			t = Mat(2, {t->nrows, t->ncols}, CV_64FC1, pt->data);
			cout << "Retrieved pose from Apriltag in image " << i << endl;
 
        images.push_back(image.clone());
        Rs.push_back(R.clone());
        Ts.push_back(T.clone());
        Rs0.push_back(R.clone());
        Ts0.push_back(T.clone());

		// Free everything to prevent memory leaks
		matd_destroy(pR);
		matd_destroy(pt);
		image_u8_destroy(byte_image);
		apriltag_detections_destroy(detections);
    }
    HostMem cret(images[0].rows,images[0].cols,CV_32FC1);
    ret=cret.createMatHeader();
    //Setup camera matrix
    double sx=reconstructionScale;
    double sy=reconstructionScale;
    cameraMatrix+=(Mat)(Mat_<double>(3,3) <<    0.0,0.0,0.5,
                                                0.0,0.0,0.5,
                                                0.0,0.0,0.0);
	// TODO: This Camera matrix is wrong?
    cameraMatrix=cameraMatrix.mul((Mat)(Mat_<double>(3,3) <<    sx,0.0,sx,
                                                                0.0,sy ,sy,
                                                                0.0,0.0,1.0));
    cameraMatrix-=(Mat)(Mat_<double>(3,3) <<    0.0,0.0,0.5,
                                                0.0,0.0,0.5,
                                                0.0,0.0,0);
    int layers=32;
    int imagesPerCV=20;
    CostVolume cv(images[0],(FrameID)0,layers,0.015,0.0,Rs[0],Ts[0],cameraMatrix);;

//     //New Way (Needs work)
//     OpenDTAM odm(cameraMatrix);
//     odm.addFrameWithPose(images[0],Rs[0],Ts[0]);
//     odm.addFrameWithPose(images[10],Rs[10],Ts[10]);
//     for (int imageNum=2;imageNum<=numImg;imageNum++){
//         odm.addFrame(images[imageNum]);
//         usleep(100000);
//     }
    
    //Old Way
    int imageNum=0;
    
    int inc=1;
    
    cv::cuda::Stream s;
    
    for (int imageNum=1;imageNum<numImg;imageNum++){
        if (inc==-1 && imageNum<4){
            inc=1;
        }
        T=Ts[imageNum].clone();
        R=Rs[imageNum].clone();
        image=images[imageNum];

        if(cv.count<imagesPerCV){
            
            cv.updateCost(image, R, T);
            cudaDeviceSynchronize();
//             gpause();
//             for( int i=0;i<layers;i++){
//                 pfShow("layer",cv.downloadOldStyle(i), 0, cv::Vec2d(0, .5));
//                 usleep(1000000);
//             }
        }
        else{
            cudaDeviceSynchronize();
            //Attach optimizer
            Ptr<DepthmapDenoiseWeightedHuber> dp = createDepthmapDenoiseWeightedHuber(cv.baseImageGray,cv.cvStream);
            DepthmapDenoiseWeightedHuber& denoiser=*dp;
            Optimizer optimizer(cv);
            optimizer.initOptimization();
            GpuMat a(cv.loInd.size(),cv.loInd.type());
             cv.loInd.copyTo(a,cv.cvStream);
//            cv.cvStream.enqueueCopy(cv.loInd,a);
            GpuMat d;
            denoiser.cacheGValues();
            ret=image*0;
//             pfShow("A function", ret, 0, cv::Vec2d(0, layers));
//             pfShow("D function", ret, 0, cv::Vec2d(0, layers));
//             pfShow("A function loose", ret, 0, cv::Vec2d(0, layers));
//             pfShow("Predicted Image",ret,0,Vec2d(0,1));
//             pfShow("Actual Image",ret);
            
            cv.loInd.download(ret);
            pfShow("loInd", ret, 0, cv::Vec2d(0, layers));
//                waitKey(0);
//                gpause();
            
            

            bool doneOptimizing; int Acount=0; int QDcount=0;

            // Optimize CV loop
            do{
//                 cout<<"Theta: "<< optimizer.getTheta()<<endl;
//
//                 if(Acount==0)
//                     gpause();
               a.download(ret);
               pfShow("A function", ret, 0, cv::Vec2d(0, layers));
                
                

                for (int i = 0; i < 10; i++) {
                    d=denoiser(a,optimizer.epsilon,optimizer.getTheta());
                    QDcount++;
                    
//                    denoiser._qx.download(ret);
//                    pfShow("Q function:x direction", ret, 0, cv::Vec2d(-1, 1));
//                    denoiser._qy.download(ret);
//                    pfShow("Q function:y direction", ret, 0, cv::Vec2d(-1, 1));
                   d.download(ret);
                   pfShow("D function", ret, 0, cv::Vec2d(0, layers));
                }
                doneOptimizing=optimizer.optimizeA(d,a);
                Acount++;
            }while(!doneOptimizing);
//             optimizer.lambda=.05;
//             optimizer.theta=10000;
//             optimizer.optimizeA(a,a);
            optimizer.cvStream.waitForCompletion();
            a.download(ret);
               pfShow("A function loose", ret, 0, cv::Vec2d(0, layers));
//                gpause();
//             cout<<"A iterations: "<< Acount<< "  QD iterations: "<<QDcount<<endl;
//             pfShow("Depth Solution", optimizer.depthMap(), 0, cv::Vec2d(cv.far, cv.near));
//             imwrite("outz.png",ret);
            
            Track tracker(cv);
            Mat out=optimizer.depthMap();
            double m;
            minMaxLoc(out,NULL,&m);
            tracker.depth=out*(.66*cv.near/m);

            // Track based on the next images
            if (imageNum+imagesPerCV+1>=numImg){ // if using the next imagesPerCV images will overflow, then inc = -1
                inc=-1;
            }
            imageNum-=imagesPerCV+1-inc; // if it won't overflow then use the previous imagesPerCV. if it will then use two before that
            for(int i=imageNum;i<numImg&&i<=imageNum+imagesPerCV+1;i++){
                tracker.addFrame(images[i]);
                tracker.align();
                LieToRT(tracker.pose,R,T);
                Rs[i]=R.clone();
                Ts[i]=T.clone();
                
                Mat p,tp;
                p=tracker.pose;
                tp=RTToLie(Rs0[i],Ts0[i]);
                {//debug
                    cout << "True Pose: "<< tp << endl;
                    cout << "True Delta: "<< LieSub(tp,tracker.basePose) << endl;
                    cout << "Recovered Pose: "<< p << endl;
                    cout << "Recovered Delta: "<< LieSub(p,tracker.basePose) << endl;
                    cout << "Pose Error: "<< p-tp << endl;
                }
                cout<<i<<endl;
                cout<<Rs0[i]<<Rs[i];
                reprojectCloud(images[i],images[cv.fid],tracker.depth,RTToP(Rs[cv.fid],Ts[cv.fid]),RTToP(Rs[i],Ts[i]),cameraMatrix);
            }
            cv=CostVolume(images[imageNum],(FrameID)imageNum,layers,0.015,0.0,Rs[imageNum],Ts[imageNum],cameraMatrix);
            s=optimizer.cvStream;
//             for (int imageNum=0;imageNum<numImg;imageNum=imageNum+1){
//                 reprojectCloud(images[imageNum],images[0],optimizer.depthMap(),RTToP(Rs[0],Ts[0]),RTToP(Rs[imageNum],Ts[imageNum]),cameraMatrix);
//             }
            a.download(ret);
            
        }
        s.waitForCompletion();// so we don't lock the whole system up forever
    }
    s.waitForCompletion();
    Stream::Null().waitForCompletion();
	tagStandard41h12_destroy(tf);
	apriltag_detector_destroy(td);
    return 0;
}


