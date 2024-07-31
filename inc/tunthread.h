#pragma once

class tunthread {

public:
    void operator()(std::atomic<bool>& running, std::string ifname);
        
};