#include "divisioninfo.h"
#include <QtGui/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	DivisionInfo w;
	w.show();
	return a.exec();
}