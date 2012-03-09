#include "qtgigecam.h"

#include "pyloncam.h"


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
	m_camera = NULL;

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
	connect(ui.actionScale, SIGNAL(triggered()), this, SLOT(toggleScaling()));


	m_pStatus = new QLabel(this);
	m_pStatus->setAlignment(Qt::AlignCenter | Qt::AlignLeft);
	m_pStatus->setText("0.0 fps  ");
	ui.statusBar->addPermanentWidget(m_pStatus);

	ui.actionStop->setEnabled(false);
	ui.actionStart->setEnabled(true);
	m_scaling = ui.actionScale->isChecked();
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

	m_camera = new PylonCam();

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

	if (!m_captureThread) {
		m_captureThread = new CaptureThread();

		if (!m_captureThread)
			return;
	}

	connect(m_captureThread, SIGNAL(newImage(Mat)), this, SLOT(newImage(Mat)), Qt::DirectConnection);

	if (m_captureThread->startCapture(m_camera)) {
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
		disconnect(m_captureThread, SIGNAL(newImage(Mat)), this, SLOT(newImage(Mat)));
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

void QtGigECam::newImage(Mat frame)
{
	m_frameCount++;

	m_frameQMutex.lock();

	if (m_frameQueue.empty())
		m_frameQueue.enqueue(frame);

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
		Mat frame;

		m_frameQMutex.lock();

		if (!m_frameQueue.empty())
			frame = m_frameQueue.dequeue();

		m_frameQMutex.unlock();

		if (!frame.empty())
			showImage(&frame);
	}
}

void QtGigECam::showImage(Mat *frame)
{	
	if (isMinimized()) 
		return;

	QImage img((const uchar *) frame->data, frame->cols, frame->rows, frame->step, QImage::Format_RGB888);
	//QImage swappedImg = img.rgbSwapped();

	if (m_scaling) {
		QImage scaledImg = img.scaled(m_cameraView->size(), Qt::KeepAspectRatioByExpanding);
		m_cameraView->setPixmap(QPixmap::fromImage(scaledImg));
	}
	else {
		m_cameraView->setPixmap(QPixmap::fromImage(img));	
	}
}

