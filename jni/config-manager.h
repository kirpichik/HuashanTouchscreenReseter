
#include <string>
#include <map>

namespace config {
  
  class Config {
  public:    
    /** Перезагружает конфигурацию с диска. 
     * @return true, если перезагрузка успешна. 
     * */
    bool reloadConfig();

    /** Проверяет наличие элемента */
    bool has(const std::string& key);

    /** Пытается получить значение строки.
     * Если такое значение отсутствует, возвращает пустую строку.
     * @param key Ключ параметра
     * @return Значение по ключу или пустую строку.
     * */
    std::string get(const std::string& key);

    /** Создает на куче объект конфига и загружает конфигурацию с диска.
     * Если загрузить конфигурацию не удалось, возвращает nullptr.
     * @param name Путь до файла конфигурации.
     * @return Указатель на объект конфигурации или NULL.
     * */
    static Config* loadConfig(const std::string& name);

  private:
    std::map<std::string, std::string> config;
    const std::string filename;

    /** 
     * @param name Путь до файла конфигурации.
     * */
    Config(const std::string& name) : filename(name) {}
  };
}

