
#include "cv.h" 
#include "highgui.h" 
#include <time.h> 
#include <math.h> 
#include <stdio.h> 
#include <string.h> 

// various tracking parameters (in seconds) //���ٵĲ�������λΪ�룩 
const double MHI_DURATION = 0.5;//0.5sΪ�˶����ٵ�������ʱ�� 
const double MAX_TIME_DELTA = 0.5; //���ʱ������Ϊ0.5s
const double MIN_TIME_DELTA = 0.05; //��Сʱ������0.05s
const int N = 3;
// 
const int CONTOUR_MAX_AERA = 900;

// ring image buffer Ȧ��ͼ�񻺳� 
IplImage **buf = 0;//ָ���ָ�� 
int last = 0;

// temporary images��ʱͼ�� 
IplImage *mhi = 0; // MHI: motion history image �˶���ʷͼ��


CvConnectedComp *cur_comp, min_comp; //���Ӳ���
CvConnectedComp comp;
CvMemStorage *storage; //�ڴ�洢��
CvPoint pt[4]; //��ά����ϵ�µĵ㣬����Ϊ���� ��ͨ����0��Ϊԭ�㣬��x�����y����

int nCurFrameIndex = 0;

// ������ 
// img - ������Ƶ֡ // dst - ����� 
void update_mhi(IplImage* img, IplImage* dst, int diff_threshold)
{
	double timestamp = clock() / 100.; // get current time in seconds ʱ��� 
	CvSize size = cvSize(img->width, img->height); // get current frame size���õ���ǰ֡�ĳߴ� 
	int i, idx1, idx2;
	IplImage* silh;
	IplImage* pyr = cvCreateImage(cvSize((size.width & -2) / 2, (size.height & -2) / 2), 8, 1);
	CvMemStorage *stor;
	CvSeq *cont;

	/*�Ƚ������ݵĳ�ʼ��*/
	if (!mhi || mhi->width != size.width || mhi->height != size.height)
	{
		if (buf == 0) //����û�г�ʼ��������ڴ���� 
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

	/*����ǰҪ�����֡ת��Ϊ�Ҷȷŵ�buffer�����һ֡��*/
	cvCvtColor(img, buf[last], CV_BGR2GRAY); // convert frame to grayscale 

	/*�趨֡�����*/
	idx1 = last;
	idx2 = (last + 1) % N; // index of (last - (N-1))th frame 
	last = idx2;

	// ��֡�� 
	silh = buf[idx2];//��ֵ��ָ��idx2 
	cvAbsDiff(buf[idx1], buf[idx2], silh); // get difference between frames 

	// �Բ�ͼ������ֵ�� 
	cvThreshold(silh, silh, 50, 255, CV_THRESH_BINARY); //threshold it,��ֵ�� 

	//ȥ����ʱ��Ӱ���Ը����˶���ʷͼ��
	cvUpdateMotionHistory(silh, mhi, timestamp, MHI_DURATION); // update MHI 
	cvShowImage("frame", mhi);
	cvConvert(mhi, dst);//��mhiת��Ϊdst,dst=mhi 

	// ��ֵ�˲�������С������ 
	cvSmooth(dst, dst, CV_MEDIAN, 3, 0, 0, 0);

	cvPyrDown(dst, pyr, CV_GAUSSIAN_5x5);// ���²�����ȥ��������ͼ����ԭͼ����ķ�֮һ 
	cvDilate(pyr, pyr, 0, 1); // �����Ͳ���������Ŀ��Ĳ������ն� 
	cvPyrUp(pyr, dst, CV_GAUSSIAN_5x5);// ���ϲ������ָ�ͼ��ͼ����ԭͼ����ı� 

	// ����ĳ���������ҵ����� 
	// Create dynamic structure and sequence. 
	stor = cvCreateMemStorage(0);
	cont = cvCreateSeq(CV_SEQ_ELTYPE_POINT, sizeof(CvSeq), sizeof(CvPoint), stor);

	// �ҵ��������� 
	cvFindContours(dst, stor, &cont, sizeof(CvContour),
		CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE, cvPoint(0, 0));

	// ֱ��ʹ��CONTOUR�еľ����������� 
	for (; cont; cont = cont->h_next)
	{
		CvRect r = ((CvContour*)cont)->rect;
		if (r.height * r.width > CONTOUR_MAX_AERA) // ���С�ķ��������� 
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
	//������Ƶ�ļ�
	//!��������Ƶ�ļ����������ó�"F://VideoSave.avi"
	char szVideoSaveName[] = "F://VideoSave.avi";

	CvVideoWriter * pVideoWriter = NULL; //���ڱ�����Ƶ�ļ�
	//IplImage * pFrame = NULL;
	//IplImage * pImage = NULL;
	IplImage* motion = 0;
	CvCapture* capture = 0;

	capture = cvCaptureFromAVI("1.avi");//AVIΪ��Ƶ��Դ 
	if (capture)
	{
		cvNamedWindow("Motion", 1);//�������� 
		cvNamedWindow("frame", 1);

		//������Ƶд����
		pVideoWriter = cvCreateVideoWriter(/*"VideoSave.avi"*/szVideoSaveName,/*CV_FOURCC                                         ('M','J', 'P', 'G')*/-1, 25, cvSize(640, 480), 1);
		for (;;)
		{
			IplImage* image;
			if (!cvGrabFrame(capture))//��׽һ�� 
				break;
			image = cvRetrieveFrame(capture);//ȡ�����֡ 
			if (image)//��ȡ�����ж�motion�Ƿ�Ϊ�� 
			{
				if (!motion)
				{
					motion = cvCreateImage(cvSize(image->width, image->height), 8, 1); //����motion֡����λ��һͨ�� 
					cvZero(motion); //�����motion 
					motion->origin = image->origin; //�ڴ�洢��˳���ȡ����֡��ͬ 
				}
			}
			update_mhi(image, motion, 60);//������ʷͼ�� 
			cvShowImage("Motion", image);//��ʾ�������ͼ�� 

			// pVideoWriter = cvCreateVideoWriter(/*"VideoSave.avi"*/szVideoSaveName,-1 ,10,cvSize(640,480),1);//����������Ƶֻ����һ֡ע��
			cvWriteFrame(pVideoWriter, image);
			// cvReleaseVideoWriter(&pVideoWriter); 
			if (cvWaitKey(33) >= 0)//10ms�а�������˳� 
				break;
		}

		cvReleaseVideoWriter(&pVideoWriter);//�ͷ�д����
		cvReleaseCapture(&capture);//�ͷ��豸
		cvDestroyWindow("Motion");//���ٴ��� 
		cvDestroyWindow("frame");//���ٴ��� 
	}
	return 0;
}