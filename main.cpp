#include "qtgigecam.h"
#include <QtGui/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	QtGigECam w;
	w.show();
	return a.exec();
}
