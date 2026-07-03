#pragma once

#include "Event.h"

#include <string>
#include <vector>

class DataHandler {
public:
    explicit DataHandler(std::string path);

    std::vector<Bar> load() const;

private:
    std::string path_;
};
