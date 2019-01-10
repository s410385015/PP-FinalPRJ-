
#include "Simulation.h"

#include <cstdio>

//  存取天氣資料的 interface 範例
class WeatherGetter : public InputInterface {
public:
    virtual WeatherStatus GetWeather(CoreType simulation_length, CoreType current_time) override {
        //  每個時刻都是雨天
        return RAIN;
    }
}weather_getter;    //  宣告一個實體

void PrintOutput(SimulationOutput& output) {
    printf("-----------------\nTotal people = %llu\nTransder = %llu\nStarve = %llu\nLose Umbrella = %llu\n",
        output.people_count, output.transfer_count, output.starve_count, output.lose_umbre_count);
}

SimulationInput input;

int main() {
    //  設定 input 資料
    input.Initialize(1, 1, 1, 1);
    //  自動分配矩陣記憶體
    input.Allocate();

    //  設定 input 中的矩陣資料
    //  ...

    //  設定存取天氣的結構
    input.input_interface = &weather_getter;

    //  設定輸出 callback 函數
    input.output_function = PrintOutput;

    //  開始模擬
    Simulate(input);

    return 0;
}
