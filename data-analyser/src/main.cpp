#include <QApplication>
#include "seizure_analyzer.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    SeizureAnalyzer window;
    window.show();
    
    return app.exec();
}
