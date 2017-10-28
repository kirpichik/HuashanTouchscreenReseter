#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/file.h>
#include <errno.h>
#include <wait.h>
#include <pthread.h>

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

#include "config-manager.h"

const std::string CALIBRATION_ON_POWER_CONFIG = "calibration-on-power";

// Код легкого нажатия кнопки камеры
const int CAMERA_LIGHT_PRESS_KEY = 528;
// Код полного нажатия кнопки камеры
const int CAMERA_FULL_PRESS_KEY = 766;

// Номер /dev/input/eventX для кнопки камеры
const int CAMERA_EVENT_ID = 1;

// Код нажатия кнопки питания
const int POWER_PRESS_KEY = 116;

// Номер /dev/input/eventX для кнопки питания
const int POWER_EVENT_ID = 0;

// Путь для манипуляций с драйвером
const std::string DRIVER_PATH = "/sys/devices/i2c-3/3-0024/main_ttsp_core.cyttsp4_i2c_adapter/";


template<typename T>
std::string toString(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

/** 
 * Вызов hw_reset у драйвера.
 * */
void callHwReset() {
  std::ofstream output;
  output.open((DRIVER_PATH + "hw_reset").c_str());
  output << "1";
  output.close();
}

/**
 * Вызов calibration у драйвера. 
 * */
void callCalibration() {
  std::ifstream input;
  input.open((DRIVER_PATH + "calibration").c_str());
  char c;
  input >> c;
  input.close();
}

/**
 * Ожидание события нажатия клавиши.
 * @param inputEventFile Файл для ожидания событий.
 * @return Код нажатой клавиши.
 * */
int waitButtonPress(int inputEventFile) {
  struct input_event event;
  
  while (1) {
    read(inputEventFile, &event, sizeof(struct input_event));

    if (event.type == 1 && event.value)
      return event.code;
  }
}

/**
 * Проверка включенного DEEP SLEEP MODE 
 * @return true, если DEEP SLEEP MODE активен. 
 * */
bool isSleepModeEnabled() {
  std::ifstream input;
  input.open((DRIVER_PATH + "sleep_status").c_str());
  std::string line;
  std::getline(input, line);
  input.close();
  return line.length() == 21; // Эквивалентно: "Deep Sleep is ENABLED".length
}

/**
 * Ожидание выключения DEEP SLEEP MODE
 * */
void waitExitSleepMode() {
  while(1) {
    if (!isSleepModeEnabled())
      return;
    usleep(200000); // 200 миллисекунд
  }
}

/**
 * Поток для получения нажатия кнопки питания.
 * */
void* powerButtonThread(void*) {
  int inputEventFile = open(("/dev/input/event" + toString(POWER_EVENT_ID)).c_str(), O_RDONLY);

  while(true) {
    int key = 0;
    while(key != POWER_PRESS_KEY)
      key = waitButtonPress(inputEventFile);

    if (!isSleepModeEnabled())
      continue;

    waitExitSleepMode();

    usleep(500000);

    callCalibration();

  }

  pthread_exit(0);
  return NULL;
}

/**
 * Поток для получения нажатия кнопки камеры.
 * */
void* cameraButtonThread(void*) {
  int inputEventFile = open(("/dev/input/event" + toString(CAMERA_EVENT_ID)).c_str(), O_RDONLY);

  while(true) {
    int key = 0;
    while(key != CAMERA_LIGHT_PRESS_KEY && key != CAMERA_FULL_PRESS_KEY)
      key = waitButtonPress(inputEventFile);
  
    if (isSleepModeEnabled())
      continue;

    if (key == CAMERA_FULL_PRESS_KEY) {
      callHwReset();
      sleep(1);
    }

    callCalibration();

  }

  pthread_exit(0);
  return NULL;
}

int workProcess(bool calibrationOnPower) {
  pthread_t cameraThread;
  pthread_t powerThread;

  pthread_attr_t attr;
  pthread_attr_init(&attr);

  pthread_create(&cameraThread, &attr, cameraButtonThread, NULL);

  if (calibrationOnPower)
    pthread_create(&powerThread, &attr, powerButtonThread, NULL);
  
  pthread_join(cameraThread, NULL);

  return 0;
}

int workMode(bool calibrationOnPower) {
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

    return workProcess(calibrationOnPower);
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

  // Получаем все параметры конфига
  bool calibrationOnPower = config->has(CALIBRATION_ON_POWER_CONFIG) 
    && config->get(CALIBRATION_ON_POWER_CONFIG) == "true";
  
  std::cout << "Config loaded successfully." << std::endl;

  int result = workMode(calibrationOnPower);
  
  delete config;

  return result;
}

