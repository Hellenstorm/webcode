
#include "cv.h" 
#include "highgui.h" 
#include <time.h> 
#include <math.h> 
#include <stdio.h> 
#include <string.h> 

// various tracking parameters (in seconds) //跟踪的参数（单位为秒） 
const double MHI_DURATION = 0.5;//0.5s为运动跟踪的最大持续时间 
const double MAX_TIME_DELTA = 0.5; //最大时间增量为0.5s
const double MIN_TIME_DELTA = 0.05; //最小时间增量0.05s
const int N = 3;
// 
const int CONTOUR_MAX_AERA = 900;

// ring image buffer 圈出图像缓冲 
IplImage **buf = 0;//指针的指针 
int last = 0;

// temporary images临时图像 
IplImage *mhi = 0; // MHI: motion history image 运动历史图像


CvConnectedComp *cur_comp, min_comp; //连接部件
CvConnectedComp comp;
CvMemStorage *storage; //内存存储器
CvPoint pt[4]; //二维坐标系下的点，类型为整型 ，通常以0点为原点，有x坐标和y坐标

int nCurFrameIndex = 0;

// 参数： 
// img - 输入视频帧 // dst - 检测结果 
void update_mhi(IplImage* img, IplImage* dst, int diff_threshold)
{
	double timestamp = clock() / 100.; // get current time in seconds 时间戳 
	CvSize size = cvSize(img->width, img->height); // get current frame size，得到当前帧的尺寸 
	int i, idx1, idx2;
	IplImage* silh;
	IplImage* pyr = cvCreateImage(cvSize((size.width & -2) / 2, (size.height & -2) / 2), 8, 1);
	CvMemStorage *stor;
	CvSeq *cont;

	/*先进行数据的初始化*/
	if (!mhi || mhi->width != size.width || mhi->height != size.height)
	{
		if (buf == 0) //若尚没有初始化则分配内存给他 
		{
			buf = (IplImage**)malloc(N*sizeof(buf[0]));
			memset(buf, 0, N*sizeof(buf[0]));
		}

		for (i = 0; i < N; i++)
		{
			cvReleaseImage(&buf[i]);
			buf[i] = cvCreateImage(size, IPL_DEPTH_8U, 1);
			cvZero(buf[i]);// clear Buffer Frame at the beginning 
		}
		cvReleaseImage(&mhi);
		mhi = cvCreateImage(size, IPL_DEPTH_32F, 1);
		cvZero(mhi); // clear MHI at the beginning 
	} // end of if(mhi) 

	/*将当前要处理的帧转化为灰度放到buffer的最后一帧中*/
	cvCvtColor(img, buf[last], CV_BGR2GRAY); // convert frame to grayscale 

	/*设定帧的序号*/
	idx1 = last;
	idx2 = (last + 1) % N; // index of (last - (N-1))th frame 
	last = idx2;

	// 做帧差 
	silh = buf[idx2];//差值的指向idx2 
	cvAbsDiff(buf[idx1], buf[idx2], silh); // get difference between frames 

	// 对差图像做二值化 
	cvThreshold(silh, silh, 50, 255, CV_THRESH_BINARY); //threshold it,二值化 

	//去掉超时的影像以更新运动历史图像
	cvUpdateMotionHistory(silh, mhi, timestamp, MHI_DURATION); // update MHI 
	cvShowImage("frame", mhi);
	cvConvert(mhi, dst);//将mhi转化为dst,dst=mhi 

	// 中值滤波，消除小的噪声 
	cvSmooth(dst, dst, CV_MEDIAN, 3, 0, 0, 0);

	cvPyrDown(dst, pyr, CV_GAUSSIAN_5x5);// 向下采样，去掉噪声，图像是原图像的四分之一 
	cvDilate(pyr, pyr, 0, 1); // 做膨胀操作，消除目标的不连续空洞 
	cvPyrUp(pyr, dst, CV_GAUSSIAN_5x5);// 向上采样，恢复图像，图像是原图像的四倍 

	// 下面的程序段用来找到轮廓 
	// Create dynamic structure and sequence. 
	stor = cvCreateMemStorage(0);
	cont = cvCreateSeq(CV_SEQ_ELTYPE_POINT, sizeof(CvSeq), sizeof(CvPoint), stor);

	// 找到所有轮廓 
	cvFindContours(dst, stor, &cont, sizeof(CvContour),
		CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE, cvPoint(0, 0));

	// 直接使用CONTOUR中的矩形来画轮廓 
	for (; cont; cont = cont->h_next)
	{
		CvRect r = ((CvContour*)cont)->rect;
		if (r.height * r.width > CONTOUR_MAX_AERA) // 面积小的方形抛弃掉 
		{
			cvRectangle(img, cvPoint(r.x, r.y),
				cvPoint(r.x + r.width, r.y + r.height),
				CV_RGB(255, 0, 0), 1, CV_AA, 0);
		}
	}
	// free memory 
	cvReleaseMemStorage(&stor);
	cvReleaseImage(&pyr);
}


int main(int argc, char** argv)
{
	//保存视频文件
	//!将保存视频文件的名字设置成"F://VideoSave.avi"
	char szVideoSaveName[] = "F://VideoSave.avi";

	CvVideoWriter * pVideoWriter = NULL; //用于保存视频文件
	//IplImage * pFrame = NULL;
	//IplImage * pImage = NULL;
	IplImage* motion = 0;
	CvCapture* capture = 0;

	capture = cvCaptureFromAVI("1.avi");//AVI为视频来源 
	if (capture)
	{
		cvNamedWindow("Motion", 1);//建立窗口 
		cvNamedWindow("frame", 1);

		//创建视频写入器
		pVideoWriter = cvCreateVideoWriter(/*"VideoSave.avi"*/szVideoSaveName,/*CV_FOURCC                                         ('M','J', 'P', 'G')*/-1, 25, cvSize(640, 480), 1);
		for (;;)
		{
			IplImage* image;
			if (!cvGrabFrame(capture))//捕捉一桢 
				break;
			image = cvRetrieveFrame(capture);//取出这个帧 
			if (image)//若取到则判断motion是否为空 
			{
				if (!motion)
				{
					motion = cvCreateImage(cvSize(image->width, image->height), 8, 1); //创建motion帧，八位，一通道 
					cvZero(motion); //零填充motion 
					motion->origin = image->origin; //内存存储的顺序和取出的帧相同 
				}
			}
			update_mhi(image, motion, 60);//更新历史图像 
			cvShowImage("Motion", image);//显示处理过的图像 

			// pVideoWriter = cvCreateVideoWriter(/*"VideoSave.avi"*/szVideoSaveName,-1 ,10,cvSize(640,480),1);//放在里有视频只会有一帧注意
			cvWriteFrame(pVideoWriter, image);
			// cvReleaseVideoWriter(&pVideoWriter); 
			if (cvWaitKey(33) >= 0)//10ms中按任意键退出 
				break;
		}

		cvReleaseVideoWriter(&pVideoWriter);//释放写入器
		cvReleaseCapture(&capture);//释放设备
		cvDestroyWindow("Motion");//销毁窗口 
		cvDestroyWindow("frame");//销毁窗口 
	}
	return 0;
}