#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include "utils/utils.hpp"
#include "reprojectCloud.hpp"

//debug
#include "tictoc.h"
#include "graphics.hpp"

//This reprojects a depthmap and image to another view. Pixels not predicted are set to the color of 0,0
//
// Camera Matrix modification to reproject with inverse depth:
//   Recall that the real camera matrix looks like:
// [ f      0          cx       0] [  xc  ] [ xp*zc] [xp]
// [ 0      f          cy       0]*[  yc  ]=[ yp*zc]=[yp]
// [ ?      ?          ?        ?] [  zc  ] [ ?    ] [? ]
// [ 0      0          1        0] [  wc  ] [ zc   ] [1 ]
//
// Notice that the z term of the output has been lost, so we can fill
// it in with whatever we want. To make it invertable and simple let's 
// do this:
//
// [ f      0          cx       0] [  xc  ] [ xp*zc] [  xp ]
// [ 0      f          cy       0]*[  yc  ]=[ yp*zc]=[  yp ]
// [ 0      0          0        1] [  zc  ] [ 1    ] [1/zc ]<--inverse depth!
// [ 0      0          1        0] [  1   ] [ zc   ] [  1  ]
//
// How Convienient! We got inverse depth for free!
// 
// Now lets look at how this matrix is used to reproject inverse depth to 
// another image(for compactness I'll write the elements of the camera matrix
// by index:
//
// [0 1 2 3] [      ] [0 1 2 3]-1  [xp  ] [x']
// [4 5 6 7]*[  A   ]*[4 5 6 7]   *[yp  ]=[y']
// [8 9 a b] [      ] [8 9 a b]    [1/zc] [z']
// [c d e f] [      ] [c d e f]    [1   ] [w']
//
// Now we want the camera matrix to look nice, so swap the bottom
// two rows:
// [0 1 2 3] [      ] [0 1 2 3]-1  [xp  ] [x']
// [4 5 6 7]*[  A   ]*[4 5 6 7]   *[yp  ]=[y']
// [c d e f] [      ] [c d e f]    [1   ] [w']
// [8 9 a b] [      ] [8 9 a b]    [1/zc] [z']
//
// Now we have:
// [ f*s    0     (cx-.5)*s+.5  0] [  xc  ] [ xp*zc] [  xp ]
// [ 0      f*s   (cy-.5)*s+.5  0]*[  yc  ]=[ yp*zc]=[  yp ]
// [ 0      0          1        0] [  wc  ] [ zc   ] [  1  ]
// [ 0      0          0        1] [  zc  ] [ 1    ] [1/zc ]
//
// But wait, now the output is getting divided by the wrong number,
// and the input has a 1/zc in the w slot, which is not allowed
// (only a 1 is allowed there).
// Well, overall, we have something of the form:
// [0 1 2 3] [xp  ] [x']
// [4 5 6 7]*[yp  ]=[y']
// [8 9 a b] [1   ] [w']
// [c d e f] [1/zc] [z']
// 
// So swap the last two rows and last two columns:
// [0 1 3 2] [xp  ] [x']
// [4 5 7 6]*[yp  ]=[y']
// [c d f e] [1/zc] [z']
// [8 9 b a] [1   ] [w']
//
// But we really don't care about the z of the output, so:
// [0 1 3 2] [xp  ] [x']
// [4 5 7 6]*[yp  ]=[y']
// [8 9 b a] [1/zc] [w']
//           [1   ] 
// So all we really needed to do is swap the two last columns
// and drop the last row.
// Thus ends the derivation.

using namespace cv;
using namespace std;

Mat reprojectCloud(const Mat comparison,const Mat _im, const Mat _depth, const Mat _oldPose, const Mat _newPose, const Mat _cameraMatrix){

    Mat im=_im;
    Mat_<float> depth=_depth;
    Mat oldPose=make4x4(_oldPose);
    Mat newPose=make4x4(_newPose);
    Mat cameraMatrix=make4x4(_cameraMatrix);
    Mat  proj(4,4,CV_64FC1);
    Mat_<Vec3f> xyin(im.rows,im.cols);
    Mat_<Vec2f> xyout(im.rows,im.cols);

//     cout<<cameraMatrix<<endl;
//     cout<<newPose<<endl;
//     cout<<oldPose<<endl;

    proj=cameraMatrix*newPose*oldPose.inv()*cameraMatrix.inv();
//     cout<<"True Mapping:"<<endl;
//     cout<<proj<<endl;
    
//     cout<<proj*(Mat_<double>(4,1)<<5,3,1,4)<<endl;
//     cout<<(_cameraMatrix.inv()*(Mat_<double>(3,1)<<5,3,1))/.25<<endl;
//     cout<<newPose*oldPose.inv()*(Mat_<double>(4,1)<<-2.614297589359934,  -1.970833333333333,4,1)<<endl;
//     cout<<_cameraMatrix*(Mat_<double>(3,1)<<-4.386563809008884,    -4.192947124058795,4.065032570174338)/4.065032570174338<<endl;
//     cout<<proj*(Mat_<double>(4,1)<<5,3,.25,1)/  1.016258142543584    <<endl;//should match on image 2
    
    
    
    
    
    
//     cout<<"This should be affine:"<<endl;
//     cout<<proj<<endl;
    Mat tmp=proj.colRange(2,4).clone();
    tmp.col(1).copyTo(proj.col(2));
    tmp.col(0).copyTo(proj.col(3));
    proj=proj.rowRange(0,3).clone();
//     cout<<"Proj: "<<"\n"<< proj<< endl;

    
//      //Check if conversions are rounded or truncated
//     tmp=(Mat_<double>(4,1)<<5,3,.7,1);
//     tmp.convertTo(tmp,CV_32SC1);
//     cout<<tmp<<endl;

    float* pt=(float*) (xyin.data);
    float* d=(float*) (depth.data);
    for(int i=0;i<im.rows;i++){
        for(int j=0;j<im.cols;j++,pt+=3,d++){
            pt[0]=j;
            pt[1]=i;
            pt[2]=*d;
        }
    }

    perspectiveTransform(xyin,xyout,proj);

    Mat xy;
    xyout.convertTo(xy,CV_32SC2);//rounds! 
    int* xyd=(int *)(xy.data);
    Mat_<float> xmap(im.rows,im.cols,-9999.9);//9999.9's are to guarantee that pixels are invalid 
    Mat_<float> ymap(im.rows,im.cols,-9999.9);//9999.9's are to guarantee that pixels are invalid 
    Mat_<float> zmap(im.rows,im.cols,-9999.9);//9999.9's are to guarantee that pixels are invalid 
    float* xm=(float*)(xmap.data);
    float* ym=(float*)(ymap.data);

    for(int i=0;i<im.rows;i++){
        for(int j=0;j<im.cols;j++,xyd+=2){
            if(xyd[1]<im.rows && xyd[1]>=0 && xyd[0]>=0 && xyd[0]<im.cols){
                if (zmap(xyd[1],xyd[0])<depth.at<float>(i,j)){
                    xmap(xyd[1],xyd[0])=j;
                    ymap(xyd[1],xyd[0])=i;
                    zmap(xyd[1],xyd[0])=depth.at<float>(i,j);
                }
            }
        }
    }
    
    //calculate the pullback image, with zbuffering to determine occlusion
    Mat xyLayers[2];
    split(xyout,xyLayers);
    xyLayers[0].reshape(1,im.rows);
    xyLayers[1].reshape(1,im.rows);
    Mat pullback;
    Mat occluded(im.rows,im.cols,CV_8UC1);
     Mat depthPullback;
//      pfShow("zmap",zmap);

     remap( zmap, depthPullback, xyLayers[0], xyLayers[1], INTER_NEAREST, BORDER_CONSTANT, Scalar(0,0, 0) );

     //do a depth test
     Mat zthr,zdiff;
     absdiff(depthPullback,depth,zdiff);
     zthr=(zdiff<.001);
     cvtColor(zthr,zthr,cv::COLOR_GRAY2BGR,3);
     zthr.convertTo(zthr,CV_32FC3,1/255.0);
    
    
//      pfShow("Occlusion",zdiff,0,Vec2d(0,.015/32));
    
     remap( comparison, pullback, xyLayers[0], xyLayers[1], cv::INTER_NEAREST, BORDER_CONSTANT, Scalar(0,0, 0) );
     Mat photoerr,pthr;
     absdiff(im,pullback,photoerr);

     cvtColor(photoerr,photoerr,cv::COLOR_BGR2GRAY);
     pthr=photoerr>.1;
     cvtColor(pthr,pthr,cv::COLOR_GRAY2RGB);
     pthr.convertTo(pthr,CV_32FC3,1/255.0);

 //     pullback.convertTo(pullback,CV_32FC3,1/255.0);
 //     CV_Assert(
     pfShow("photo Error",photoerr);


     Mat confidence;
     sqrt(pthr,confidence);
     confidence=Scalar(1,1,1)-confidence;
     pullback=pullback.mul(confidence).mul(zthr);
//      pfShow("Stabilized Projection",pullback,0,Vec2d(0,1));
    static Mat fwdp2;
    static Mat fwdp=im.clone();
    remap( im, fwdp, xmap, ymap, INTER_NEAREST, BORDER_CONSTANT,Scalar(0,0,0));
//     medianBlur(fwdp,fwdp2,3);
// //     remap( im, fwdp, xmap, ymap, INTER_NEAREST, BORDER_TRANSPARENT);
//     
//     fwdp2.copyTo(fwdp,fwdp==0);
//     medianBlur(fwdp,fwdp2,3);
//     fwdp2.copyTo(fwdp,fwdp==0);
    pfShow("Predicted Image",fwdp,0,Vec2d(0,1));
//     absdiff(fwdp,comparison,zdiff);
    pfShow("Actual Image",comparison);
   
    

    
//     pfShow("diff",zdiff.mul(fwdp));
    



    return xyout;
    
}

