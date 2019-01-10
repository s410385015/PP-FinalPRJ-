
/*
    定義雨傘模擬器的輸入、輸出格式
    除了需要常駐於記憶體中的資料外，提供一個讀取的 interface 來抽象化存取過程
    需要常駐於記憶體的資料：
        *   模擬時間長度 T
        *   人的種類數量 K
        *   每種人帶傘的比率(僅初始化需要) u[i]
        *   建築數量 B
        *   每種人在建築之間移動的規律 p[k][r][i][j]
        *   移動規律的週期 R
        *   初始狀態每棟建築人口分布 d[b][k]
        *   輸出 callback 函數

    不需要常駐於記憶體(Ex:與時間相關)的資料
        *   每個時間點的天氣狀態
    
    模擬過程在每個時間刻都會回傳以下資料
        *   模擬總人口數
        *   多少人在上個時間點進行建築轉移
        *   多少人在上個時間點無傘可用
        *   在無傘可用的人當中，多少人在進建築的時候是有傘可用的
        *   各個建築的狀態
*/

#include <vector>

//  CoreType 
//  用於計算人口數，和各種其他 count
typedef unsigned long long CoreType;

namespace {
    const unsigned int cache_line_size = 64;
}

//  描述單一個人的狀態
struct Person {
    CoreType kind_id;   //  屬於哪一個人種
    bool own_umbre;     //  進入建築時是否有帶傘
};

//  描述單一建築的狀態
struct Building {
    CoreType umbre_count;               //  剩下多少傘
    std::vector<Person> person_list;    //  建築裡有哪些人
    
    //  填滿 cache line
    // unsigned char cache_line_fill[cache_line_size-sizeof(CoreType)-sizeof(std::vector<Person>)];
};

//  定義模擬器輸出格式
struct SimulationOutput {
    CoreType current_time;
    CoreType people_count;      //  模擬總人口數
    CoreType transfer_count;    //  多少人在上個時間點進行建築轉移
    CoreType starve_count;      //  多少人在上個時間點無傘可用
    CoreType lose_umbre_count;  //  在無傘可用的人當中，多少人在進建築的時候是有傘可用的

    CoreType building_count;    //  總共有多少建築
    Building *building_status;   //  各個建築的狀態
};

//  窮舉天氣狀態
enum WeatherStatus {
    CLOUDY = 0,
    RAIN
};

//  定義取得不需要常駐於記憶體的資料之接口
class InputInterface {
public:
    virtual WeatherStatus GetWeather(CoreType simulation_length, CoreType current_time) = 0;
};

//  OutputFunction
//  模擬器輸出時使用
typedef void(*OutputFunction)(SimulationOutput&);

//  定義模擬器輸入格式
class SimulationInput {
public:
    CoreType simulation_length;         //  模擬時間長度 T
    CoreType human_kind_count;          //  人的種類數量 K
    double *human_kind_carry_rate;       //  每種人帶傘的比率
    CoreType building_count;            //  建築數量 B
    double ****move_rule;               //  每種人在建築之間移動的規律 p[k][r][i][j]
    CoreType move_rule_period;
    CoreType **building_people_count;     //  初始狀態每棟建築人口分布
    
    InputInterface* input_interface;    //  不需要常駐於記憶體的資料
    
    OutputFunction output_function;

    void Initialize(CoreType simulation_length_, CoreType human_kind_count_, CoreType building_count_, CoreType move_rule_period_) {
        simulation_length = simulation_length_;
        human_kind_count = human_kind_count_;
        building_count = building_count_;
        move_rule_period = move_rule_period_;
    }
    void Allocate() {
        human_kind_carry_rate = new double[human_kind_count];
        move_rule = new double***[human_kind_count];
        for(int i = 0; i < human_kind_count; ++i) {
            move_rule[i] = new double**[move_rule_period];
            for(int j = 0; j < move_rule_period; ++j) {
                move_rule[i][j] = new double*[building_count];
                for(int k = 0; k < building_count; ++k)
                    move_rule[i][j][k] = new double[building_count];
            }
        }
        building_people_count = new CoreType*[building_count];
        for(int i = 0; i < building_count; ++i)
            building_people_count[i] = new CoreType[building_count];
    }
};

