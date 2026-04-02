#include <QApplication>
#include "main-window.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  main_window_t window;
  window.show();

  return app.exec();
}
