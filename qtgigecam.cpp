#include "qtgigecam.h"

//#define WEBCAM

QtGigECam::QtGigECam(QWidget *parent, Qt::WFlags flags)
	: QMainWindow(parent, flags)
{
	ui.setupUi(this);

	m_frameCount = 0;
	m_captureThread = NULL;
	m_frameRateTimer = 0;
	m_width = 0;
	m_height = 0;
	m_pitch = 0;

	connect(ui.actionExit, SIGNAL(triggered()), this, SLOT(close()));
	connect(ui.actionStart, SIGNAL(triggered()), this, SLOT(startVideo()));
	connect(ui.actionStop, SIGNAL(triggered()), this, SLOT(stopVideo()));

	m_pStatus = new QLabel(this);
	m_pStatus->setAlignment(Qt::AlignCenter | Qt::AlignLeft);
	m_pStatus->setText("0.0 fps  ");
	ui.statusBar->addPermanentWidget(m_pStatus);

	ui.actionStop->setEnabled(false);
	ui.actionStart->setEnabled(true);

	m_milApp = MappAlloc(M_DEFAULT, M_NULL);

	prepareBuffers(4);
}

QtGigECam::~QtGigECam()
{
	if (m_captureThread) {
		stopVideo();
		m_captureThread->disconnectCamera();
		delete m_captureThread;
		m_captureThread = NULL;
	}

	freeBuffers();

	if (m_milApp)
		MappFree(m_milApp);
}

void QtGigECam::startVideo()
{
	if (!m_captureThread) {
#ifdef WEBCAM
		m_captureThread = new CaptureThread(this, ctWebCam);
#else
		m_captureThread = new CaptureThread(this, ctPylonCam);
#endif

		if (!m_captureThread)
			return;
	}

	prepareQueues();

	if (m_captureThread->startCapture()) {
		m_frameCount = 0;
		m_frameRateTimer = startTimer(3000);
		m_frameRefreshTimer = startTimer(30);
		ui.actionStart->setEnabled(false);
		ui.actionStop->setEnabled(true);
	}
}

void QtGigECam::stopVideo()
{
	if (m_captureThread)
		m_captureThread->stopCapture();

	killTimer(m_frameRateTimer);
	killTimer(m_frameRefreshTimer);

	ui.actionStop->setEnabled(false);
	ui.actionStart->setEnabled(true);
}

bool QtGigECam::setGrabFrame(MIL_ID buf_id)
{
	m_frameCount++;

	if (!m_frameQMutex.tryLock())
		return false;
	
	m_frameQueue.enqueue(buf_id);
	m_frameQMutex.unlock();

	return true;
}

MIL_ID QtGigECam::getFreeBuffer()
{
	MIL_ID buf_id = 0;

	if (!m_freeQMutex.tryLock())
		return 0;

	if (!m_freeQueue.empty())
		buf_id = m_freeQueue.dequeue();

	m_freeQMutex.unlock();

	return buf_id;
}

void QtGigECam::timerEvent(QTimerEvent *event)
{
	if (event->timerId() == m_frameRateTimer) {
		QString fps;
		double count = m_frameCount;
		m_frameCount = 0;
		fps.sprintf("%0.1lf fps  ", count / 3.0);
		m_pStatus->setText(fps);		
	}
	else {
		m_frameQMutex.lock();
		if (m_frameQueue.empty()) {
			m_frameQMutex.unlock();
		}
		else {
			MIL_ID buf_id = m_frameQueue.dequeue();
			m_frameQMutex.unlock();

			showImage(buf_id);

			m_freeQMutex.lock();
			m_freeQueue.enqueue(buf_id);
			m_freeQMutex.unlock();
		}
	}
}

void QtGigECam::showImage(MIL_ID buf_id)
{	
	uchar *buff;

	if (isMinimized()) 
		return;

	buff = (uchar *) MbufInquire(buf_id, M_HOST_ADDRESS, NULL);

	QImage img((const uchar*) buff, m_width, m_height, m_pitch, QImage::Format_RGB888);

//#ifdef WEBCAM
	QImage swappedImg = img.rgbSwapped();
	ui.cameraView->setPixmap(QPixmap::fromImage(swappedImg));
//#else
//	ui.cameraView->setPixmap(QPixmap::fromImage(img));
//#endif
}

void QtGigECam::prepareBuffers(int numBuffers)
{
	MIL_ID buf_id = 0;

	freeBuffers();

	for (int i = 0; i < numBuffers; i++) {
#ifdef WEBCAM
		buf_id = MbufAllocColor(M_DEFAULT_HOST, 3, 640, 480, 8 + M_UNSIGNED, M_IMAGE + M_PROC + M_BGR24 + M_PACKED, M_NULL);
#else
		buf_id = MbufAllocColor(M_DEFAULT_HOST, 3, 1278, 958, 8 + M_UNSIGNED, M_IMAGE + M_PROC + M_BGR24 + M_PACKED, M_NULL);
#endif
		m_freeQueue.enqueue(buf_id);
	}

	if (m_pitch == 0) {
		m_pitch = MbufInquire(buf_id, M_PITCH_BYTE, NULL);
		m_width = MbufInquire(buf_id, M_SIZE_X, NULL);
		m_height = MbufInquire(buf_id, M_SIZE_Y, NULL);
	}
}

void QtGigECam::freeBuffers()
{
	MIL_ID buf_id;

	m_freeQMutex.lock();
	m_frameQMutex.lock();

	while (!m_frameQueue.empty()) {
		buf_id = m_frameQueue.dequeue();
		MbufFree(buf_id);
	}

	while (!m_freeQueue.empty()) {
		buf_id = m_freeQueue.dequeue();
		MbufFree(buf_id);
	}

	m_frameQMutex.unlock();
	m_freeQMutex.unlock();
}

void QtGigECam::prepareQueues()
{
	m_freeQMutex.lock();
	m_frameQMutex.lock();

	while (!m_frameQueue.empty()) {
		MIL_ID buf_id = m_frameQueue.dequeue();
		m_freeQueue.enqueue(buf_id);
	}

	m_frameQMutex.unlock();
	m_freeQMutex.unlock();
}
