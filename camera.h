#ifndef CAMERA_H
#define CAMERA_H

#include <QThread>
#include <Mil.h>

class Camera : protected QThread
{
public:
	Camera() {}
	virtual ~Camera() {}

	virtual bool open() = 0;
	virtual void close() = 0;
	virtual bool isOpen() = 0;
	virtual bool startCapture() { return true; }
	virtual bool stopCapture() { return true; }
	virtual bool getNextFrame(MIL_ID buf_id) = 0;
};

#endif // CAMERA_H
