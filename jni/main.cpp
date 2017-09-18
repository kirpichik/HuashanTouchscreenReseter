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
#include <sstream>

#include "config-manager.h"

template<typename T>
std::string toString(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
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

/** Вызов hw_reset у драйвера. 
 * @param path Путь до каталога управления драйвером.
 * */
void callHwReset(const std::string& path) {
  std::ofstream output;
  output.open((path + "hw_reset").c_str());
  output << "1";
  output.close();
}

/** Вызов calibration у драйвера. 
 * @param path Путь до каталога управления драйвером.
 * */
void callCalibration(const std::string& path) {
  std::ifstream input;
  input.open((path + "calibration").c_str());
  char c;
  input >> c;
  input.close();
}

/** Ожидание события нажатия клавиши.
 * @param inputEventFile Файл для ожидания событий.
 * @param lightKey Код легкого нажатия кнопки камеры.
 * @param fullKey Код полного нажатия кнопки камеры. 
 * @return Код нажатой клавиши.
 * */
int waitButtonPress(int inputEventFile, int lightKey, int fullKey) {
  struct input_event event;
  
  while (1) {
    read(inputEventFile, &event, sizeof(struct input_event));

    if (event.type == 1 && event.value && (event.code == lightKey || event.code == fullKey))
      return event.code;
  }
}

/** Проверка включенного DEEP SLEEP MODE 
 * @param path Путь до каталога управления драйвером.
 * @return true, если DEEP SLEEP MODE активен. 
 * */
bool isSleepModeEnabled(const std::string& path) {
  std::ifstream input;
  input.open((path + "sleep_status").c_str());
  std::string line;
  std::getline(input, line);
  input.close();
  return line.length() == 21; // Эквивалентно: "Deep Sleep is ENABLED".length
}

/** Ожидание выключения DEEP SLEEP MODE */
/*void waitExitSleepMode(std::string path) {
  while(1) {
    if (!isSleepModeEnabled(path))
      return;
    sleep(1); // TODO - Уменьшить время перепроверки.
  }
}*/

int workProcess(std::string driverPath, int eventId, int lightKey, int fullKey) {

  int inputEventFile = open(("/dev/input/event" + toString(eventId)).c_str(), O_RDONLY);

  while(1) {
    int key = waitButtonPress(inputEventFile, lightKey, fullKey);

    if (isSleepModeEnabled(driverPath))
      continue;
    
    if (key == fullKey) {
      callHwReset(driverPath);
      sleep(1);
    }

    callCalibration(driverPath);
  }

  //close(inputEventFile); ?

  return 0;
}

/** Рабочий режим */
int workMode(std::string driverPath, int eventId, int lightKey, int fullKey) {
  std::cout << "Work mode started." << std::endl;
  
  // Начинаем запуск демона
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

    std::cout << "Daemon started." << std::endl;

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    return workProcess(driverPath, eventId, lightKey, fullKey);
  }

  return 0;
}

/** Режим отладки настроек конфига */
int debugMode(std::string driverPath, int eventId, int lightKey, int fullKey) {
  std::cout << "Debug mode started." << std::endl;
  
  struct stat st;
  if (stat(driverPath.c_str(), &st) == -1) {
    std::cout << "Cannot find driver-path." << std::endl;
    return 1;
  }
  
  std::cout << "Driver path found." << std::endl;

  if (stat(("/dev/input/event" + toString(eventId)).c_str(), &st) == -1) {
    std::cout << "Cannot find /dev/input/event" << eventId << std::endl;
    return 1;
  }

  int inputEventFile = open(("/dev/input/event" + toString(eventId)).c_str(), O_RDONLY);
  
  std::cout << "Button test:" << std::endl;

  while(1) {
    int key = waitButtonPress(inputEventFile, lightKey, fullKey);

    if (key == lightKey)
      std::cout << "Light key touch" << std::endl;
    else
      std::cout << "Full key touch" << std::endl;
  }

  return 0;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << "Usage: ./touchsreen-reseter filename.conf" << std::endl;
    return 1;
  }
  
  // Загружаем конфиг
  config::Config* config = config::Config::loadConfig(argv[1]);
  if (!config) {
    std::cout << "Error: Load config failed." << std::endl;
    return 1;
  }

  // Узнаем, в каком режиме нужно запуститься.
  bool debug = config->has("debug") && config->get("debug") == "true";

  // Проверяем наличие всех параметров в конфиге
  if (!config->has("driver-path") 
      || !config->has("event-ID") 
      || !config->has("light-touch-key") 
      || !config->has("full-touch-key")) {
    std::cout << "Wrong config." << std::endl;
    return 1;
  }

  // Получаем все параметры конфига
  std::string driverPath = config->get("driver-path");
  if (driverPath[driverPath.length() - 1] != '/')
    driverPath += "/";
  int eventId = atoi(config->get("event-ID").c_str());
  int lightKey = atoi(config->get("light-touch-key").c_str());
  int fullKey = atoi(config->get("full-touch-key").c_str());

  std::cout << "Config loaded successfully." << std::endl;

  int result = debug ? debugMode(driverPath, eventId, lightKey, fullKey) : workMode(driverPath, eventId, lightKey, fullKey);
  
  delete config;

  return result;
}

