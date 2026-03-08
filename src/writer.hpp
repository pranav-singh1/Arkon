#pragma once
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class TickWriter {
public:
    TickWriter(const std::string& filepath);
    void write(const json& tick);
    ~TickWriter();

private:
    std::ofstream file_;
};

TickWriter::TickWriter(const std::string& filepath){
    file_.open(filepath, std::ios::out | std::ios::trunc);
    if(!file_.is_open()){
        throw std::runtime_error("Failed to open file: " + filepath);
    }
}

void TickWriter::write(const json& tick){
    file_ <<tick.dump() << "\n";
    file_.flush();//making sure the data is written to the file
}

TickWriter::~TickWriter(){
    file_.close();
}
