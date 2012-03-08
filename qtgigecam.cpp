#include "qtgigecam.h"

#include "capturethread.h"
#include "camera.h"
#include "pyloncam.h"
#include "webcam.h"


QtGigECam::QtGigECam(QWidget *parent, Qt::WFlags flags)
	: QMainWindow(parent, flags)
{
	ui.setupUi(this);

	m_frameCount = 0;
	m_captureThread = NULL;
	m_frameRateTimer = 0;
	m_frameRefreshTimer = 0;
	m_imgWidth = 0;
	m_imgHeight = 0;
	m_imgPitch = 0;
	m_cameraType = ctPylonCam;
	m_camera = NULL;
	m_camera_buff = 0;

	QWidget *centralWidget = new QWidget(this);
	QVBoxLayout *verticalLayout = new QVBoxLayout(centralWidget);
	verticalLayout->setSpacing(6);
	verticalLayout->setContentsMargins(0, 0, 0, 0);
	m_cameraView = new QLabel(centralWidget);
	
	QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	sizePolicy.setHorizontalStretch(0);
	sizePolicy.setVerticalStretch(0);
	sizePolicy.setHeightForWidth(m_cameraView->sizePolicy().hasHeightForWidth());
	m_cameraView->setSizePolicy(sizePolicy);
	m_cameraView->setMinimumSize(QSize(320, 240));
	m_cameraView->setAlignment(Qt::AlignCenter);

	verticalLayout->addWidget(m_cameraView);

	setCentralWidget(centralWidget);

	connect(ui.actionExit, SIGNAL(triggered()), this, SLOT(close()));
	connect(ui.actionStart, SIGNAL(triggered()), this, SLOT(startVideo()));
	connect(ui.actionStop, SIGNAL(triggered()), this, SLOT(stopVideo()));
	connect(ui.actionScale, SIGNAL(triggered()), this, SLOT(scaleImage()));


	m_pStatus = new QLabel(this);
	m_pStatus->setAlignment(Qt::AlignCenter | Qt::AlignLeft);
	m_pStatus->setText("0.0 fps  ");
	ui.statusBar->addPermanentWidget(m_pStatus);

	ui.actionStop->setEnabled(false);
	ui.actionStart->setEnabled(true);
	m_scaling = ui.actionScale->isChecked();

	m_milApp = MappAlloc(M_DEFAULT, M_NULL);	
}

QtGigECam::~QtGigECam()
{
	if (m_captureThread) {
		stopVideo();
		delete m_captureThread;
		m_captureThread = NULL;
	}

	if (m_camera) {
		delete m_camera;
		m_camera = NULL;
	}
		
	freeBuffers();

	if (m_milApp)
		MappFree(m_milApp);
}

void QtGigECam::toggleScaling()
{
	m_scaling = ui.actionScale->isChecked();
}

bool QtGigECam::createCamera()
{
	if (m_captureThread && m_captureThread->isRunning()) {
		stopVideo();
	}

	if (m_camera) {
		delete m_camera;
		m_camera = NULL;
	}

	switch (m_cameraType) {
	case ctWebCam:
		m_camera = new WebCam();
		break;

	case ctPylonCam:
		m_camera = new PylonCam();
		break;

	default:
		m_camera = NULL;
	}

	return (m_camera != NULL);
}

void QtGigECam::startVideo()
{
	if (!m_camera) {
		if (!createCamera())
			return;
	}

	if (!m_camera->open()) {
		delete m_camera;
		m_camera = NULL;
		return;
	}

	if (!prepareBuffers(4))
		return;

	prepareQueues();

	if (!m_captureThread) {
		m_captureThread = new CaptureThread();

		if (!m_captureThread)
			return;
	}

	connect(m_captureThread, SIGNAL(newImage(MIL_ID)), this, SLOT(newImage(MIL_ID)), Qt::DirectConnection);

	if (m_captureThread->startCapture(m_camera, m_camera_buff)) {
		m_frameCount = 0;
		m_frameRateTimer = startTimer(3000);
		m_frameRefreshTimer = startTimer(20);
		ui.actionStart->setEnabled(false);
		ui.actionStop->setEnabled(true);
	}
}

void QtGigECam::stopVideo()
{
	if (m_captureThread) {
		disconnect(m_captureThread, SIGNAL(newImage(MIL_ID)), this, SLOT(newImage(MIL_ID)));
		m_captureThread->stopCapture();
	}
	
	if (m_frameRateTimer) {
		killTimer(m_frameRateTimer);
		m_frameRateTimer = 0;
	}

	if (m_frameRefreshTimer) {
		killTimer(m_frameRefreshTimer);
		m_frameRefreshTimer = 0;
	}

	ui.actionStop->setEnabled(false);
	ui.actionStart->setEnabled(true);
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

void QtGigECam::newImage(MIL_ID src_buff)
{
	MIL_ID dest_buff;

	m_frameCount++;

	if (!m_frameQMutex.tryLock())
		return;

	dest_buff = getFreeBuffer();

	if (!dest_buff) {
		m_frameQMutex.unlock();
		return;
	}

	MbufCopy(src_buff, dest_buff);
	m_frameQueue.enqueue(dest_buff);
	m_frameQMutex.unlock();
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

	QImage img((const uchar*) buff, m_imgWidth, m_imgHeight, m_imgPitch, QImage::Format_RGB888);
	QImage swappedImg = img.rgbSwapped();

	if (m_scaling) {
		QImage scaledImg = swappedImg.scaled(m_cameraView->size(), Qt::KeepAspectRatioByExpanding);
		m_cameraView->setPixmap(QPixmap::fromImage(scaledImg));
	}
	else {
		m_cameraView->setPixmap(QPixmap::fromImage(swappedImg));	
	}
}

bool QtGigECam::prepareBuffers(int numBuffers)
{
	QSize sz;
	MIL_ID buf_id;

	sz = m_camera->getImageSize();

	// check if we are already done
	if (m_imgWidth != 0 && sz.width() == m_imgWidth && sz.height() == m_imgHeight)
		return true;

	freeBuffers();

	for (int i = 0; i < numBuffers; i++) {
		buf_id = MbufAllocColor(M_DEFAULT_HOST, 3, sz.width(), sz.height(), 8 + M_UNSIGNED, M_IMAGE + M_PROC + M_BGR24 + M_PACKED, M_NULL);
		m_freeQueue.enqueue(buf_id);
	}

	m_camera_buff = MbufAllocColor(M_DEFAULT_HOST, 3, sz.width(), sz.height(), 8 + M_UNSIGNED, M_IMAGE + M_PROC + M_BGR24 + M_PACKED, M_NULL);

	m_imgPitch = MbufInquire(buf_id, M_PITCH_BYTE, NULL);
	m_imgWidth = MbufInquire(buf_id, M_SIZE_X, NULL);
	m_imgHeight = MbufInquire(buf_id, M_SIZE_Y, NULL);

	return true;
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

	if (m_camera_buff) {
		MbufFree(m_camera_buff);
		m_camera_buff = NULL;
	}		

	m_frameQMutex.unlock();
	m_freeQMutex.unlock();
	
	m_imgPitch = 0;
	m_imgWidth = 0;
	m_imgHeight = 0;
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
