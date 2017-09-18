#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/file.h>
#include <errno.h>
#include <wait.h>

#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <vector>
#include <sstream>

#include "config-manager.h"

const std::string logFile = "log.log";

int inputEventFile = -1;

template<typename T>
std::string toString(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

namespace tsr {
  std::ofstream log;
}

/*void input() {
  int file;
  struct input_event event;

  if(argc < 2) {
    printf("usage: %s <event path> \n", argv[0]);
    return 1;
  }

  file = open(argv[1], O_RDONLY);
  while (1) {
    read(file, &event, sizeof(struct input_event));

    printf("[type=%i, code=%i, value=%i]\n", event.type, event.code, event.value);

    if (event.type == 1) {
      if (event.value) {
        printf("1:%i\n", event.code);
        fflush(stdout);
      }
      else {
        printf("0:%i\n", event.code);
        fflush(stdout);
      }
    }
  }  
}*/

/** Вызов hw_reset у драйвера. */
void callHwReset(std::string path) {
  tsr::log << "[DAEMON] Calling hw-reset..." << std::endl;
  std::ofstream output;
  output.open((path + "hw_reset").c_str());
  output << "1";
  output.close();
}

/** Вызов calibration у драйвера. */
void callCalibration(std::string path) {
  tsr::log << "[DAEMON] Calling calibration..." << std::endl;
  std::ifstream input;
  input.open((path + "calibration").c_str());
  char c;
  input >> c;
  input.close();
}

/** Ожидание события нажатия клавиши. */
void waitButtonPress() {
  struct input_event event;
  
  while (1) {
    read(inputEventFile, &event, sizeof(struct input_event));

    if (event.type == 1 && event.value)
      return;
  }
}

/** Проверка включенного DEEP SLEEP MODE */
bool isSleepModeEnabled(std::string path) {
  std::ifstream input;
  input.open((path + "sleep_status").c_str());
  std::string line;
  std::getline(input, line);
  input.close();
  return line.length() == std::string("Deep Sleep is ENABLED").length();
}

/** Ожидание выключения DEEP SLEEP MODE */
void waitExitSleepMode(std::string path) {
  while(1) {
    //tsr::log << "[DAEMON] Waiting screen fully enabled..." << std::endl;
    if (!isSleepModeEnabled(path))
      return;
    sleep(1); // TODO - Уменьшить время перепроверки.
  }
}

int workProcess(std::string driverPath, unsigned int eventId, bool needHwReset) {

  inputEventFile = open((std::string("/dev/input/event") + toString(eventId)).c_str(), O_RDONLY);

  tsr::log << "[DAEMON] Running..." << std::endl;

  while(1) {
    waitButtonPress();
    
    //tsr::log << "[DAEMON] Catched button touch." << std::endl;

    if (!isSleepModeEnabled(driverPath)) {
      //tsr::log << "[DAEMON] Sleep mode is DISABLED, ignoring it." << std::endl;
      continue;
    }

    waitExitSleepMode(driverPath);

    //callHwReset(driverPath);

    callCalibration(driverPath);
  }

  close(inputEventFile);

  tsr::log << "[DAEMON] Finished." << std::endl;

  return 0;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "Usage: ./touchsreen-reseter filename.conf" << std::endl;
    return 1;
  }
  
  config::Config* config = config::Config::loadConfig(argv[1]);
  if (!config) {
    std::cout << "Error: Load config failed." << std::endl;
    return 1;
  }

  if (!config->has("driver-path") || !config->has("event-ID") || !config->has("need-hw-reset")) {
    std::cout << "Wrong config." << std::endl;
    return;
  }

  std::string driverPath = config->get("driver-path");
  if (driverPath[driverPath.length() - 1] != '/')
    driverPath += "/";
  unsigned int eventId = atoi(config->get("event-ID").c_str());
  bool needHwReset = config->get("need-hw-reset") == "true" ? true : false;

  delete config;

  std::cout << "Config loaded successfully." << std::endl;

  int pid = fork();

  if (pid == -1) { // Не удалось запустить потомка
    std::cout << "Start Daemon Error: " << strerror(errno) << std::endl;
    return 1;
  } else if (!pid) { // Этот процесс - потомок
    // Разрешаем выставлять все биты прав на создаваемые файлы,
    // Иначе у нас могут быть проблемы с правами доступа
    umask(0);

    // Создаём новый сеанс, чтобы не зависеть от родителя
    setsid();

    // Переходим в корень диска. Если мы этого не сделаем, то могут быть проблемы,
    // к примеру с размонтированием дисков
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /*struct stat st;
    if (stat(logsPath.c_str(), &st) == -1) {
      mkdir(logsPath.c_str(), 0700);
      // TODO - Check permissions.
    }*/

    tsr::log.open(logFile.c_str());
    
    int result = workProcess(driverPath, eventId, needHwReset);

    tsr::log.close();
    
    return result;
  }

  return 0;
}

