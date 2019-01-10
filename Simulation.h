
#include "DataInterface.h"

#include <omp.h>
#include <time.h>
#include <vector>

namespace {

    
    inline unsigned int SimpleRand(unsigned int& stateRand) {
        return (stateRand=(((unsigned long long int)stateRand)*1103515245+12345)&0x7FFFFFFF);
    }
    const unsigned int rand_max = 0x7FFFFFFF;

    const unsigned int thread_count_max = 8, building_count_max = 1000;

    struct ThreadCache {
        unsigned int rand_status;
        CoreType transfer_count;
        CoreType starve_count;
        CoreType lose_umbre_count;
        std::vector<Person> building_transfer[building_count_max];

        unsigned char cache_line_fill[64];
    };
    
    ThreadCache thread_cache[thread_count_max];
}

/*
    執行雨傘模擬
*/
void Simulate(SimulationInput& input) {
    /*
        初始化
    */

    //  計算每個建築初始狀態總共多少人，同時前綴和取得總人口數
    CoreType *building_people_sum = new CoreType[input.building_count+1];
    building_people_sum[0] = 0;
    int i, j, k, l;
    //  outer max = 1000, inner max = 10, don't need PP
    for(i = 0; i < input.building_count; ++i) {
        building_people_sum[i+1] = building_people_sum[i];
        for(j = 0; j < input.human_kind_count; ++j) {
            building_people_sum[i+1] += input.building_people_count[i][j];
        }
    }

    Building* building = new Building[input.building_count];
    //  為所有人分配 building_id (以建築為單位幫建築內所有人指定 id)
    #pragma omp parallel default(none) shared(input, building)
    {
        #pragma omp for private(i, j, k)
        for(i = 0; i < input.building_count; ++i) {
            building[i].umbre_count = 0;
            for(j = 0; j < input.human_kind_count; ++j) {
                const CoreType building_people_count = input.building_people_count[i][j];
                building[i].umbre_count += building_people_count*input.human_kind_carry_rate[j];
                for(k = 0; k < building_people_count; ++k) {
                    Person person;
                    person.kind_id = j;
                    person.own_umbre = false;
                    building[i].person_list.push_back(person);
                }
            }
        }
        //  將人種的移動規則做 L1 正規化，並且變成 CDF，方便後面使用
        #pragma omp for private(i, j, k, l)
        for(i = 0; i < input.human_kind_count; ++i) {
            for(j = 0; j < input.move_rule_period; ++j) {
                for (k = 0; k < input.building_count; ++k) {
                    double sum = 0;
                    for(l = 0; l < input.building_count; ++l) {
                        sum += input.move_rule[i][j][k][l];
                    }
                    double last_rule = 0;
                    for(l = 0; l < input.building_count; ++l) {
                        input.move_rule[i][j][k][l] /= sum;
                        input.move_rule[i][j][k][l] += last_rule;
                        last_rule = input.move_rule[i][j][k][l];
                    }
                    //  保證總和機率不受浮點運算影響
                    input.move_rule[i][j][k][input.building_count-1] = 1.0;
                }
            }
        }
    }

    //  初始化 output 結構
    SimulationOutput output;
    output.current_time = 0;

    output.people_count = building_people_sum[input.building_count];
    delete[]building_people_sum;

    output.transfer_count = 0;
    output.starve_count = 0;
    output.lose_umbre_count = 0;

    output.building_count = input.building_count;
    output.building_status = building;

    //  輸出時間點 0 的資料
    input.output_function(output);

    for(i = 0; i < thread_count_max; ++i) {
        thread_cache[i].rand_status = time(NULL);
        thread_cache[i].transfer_count = 0;
        thread_cache[i].starve_count = 0;
        thread_cache[i].lose_umbre_count = 0;
    }

    //  模擬
    for(output.current_time = 1; output.current_time <= input.simulation_length; ++output.current_time) {
        CoreType time_in_period = output.current_time%input.move_rule_period;
        bool is_rain = (input.input_interface->GetWeather(input.simulation_length, output.current_time) == RAIN);
        #pragma omp parallel default(none) shared(input, output, building, time_in_period, is_rain, thread_cache)
        {
            const int thread_id = omp_get_thread_num(), thread_count = omp_get_num_threads();;
            #pragma omp for private(i, j, k)
            for(i = 0; i < input.building_count; ++i) {
                size_t person_left = building[i].person_list.size();
                j = 0;
                while(j < person_left) {
                    const double rule_index = SimpleRand(thread_cache[i].rand_status)/(double)rand_max;
                    const CoreType persion_kind = building[i].person_list[j].kind_id;
                    for(k = 0; k < input.building_count; ++k) {
                        if(rule_index <= input.move_rule[persion_kind][time_in_period][i][k]) {
                            break;
                        }
                    }
                    if(i == k) ++j;
                    else {
                        //  確認雨傘是否足夠
                        if(building[i].umbre_count == 0 && is_rain) {
                            thread_cache[thread_id].starve_count += 1;

                            if(building[i].person_list[j].own_umbre) {
                                thread_cache[thread_id].lose_umbre_count += 1;
                            }
                            ++j;
                            continue;
                        }
                        if(is_rain) {
                            --building[i].umbre_count;
                        }

                        building[i].person_list[j].own_umbre = is_rain;
                        thread_cache[thread_id].building_transfer[k].push_back(building[i].person_list[j]);
                        if(j != person_left-1) {
                            building[i].person_list[j] = building[i].person_list[person_left-1];
                        }
                        building[i].person_list.pop_back();
                        --person_left;
                    }
                }
            }
            #pragma omp for private(i, j)
            for(i = 0; i < input.building_count; ++i) {
                for(j = 0; j < thread_count; ++j) {
                    building[i].person_list.insert(building[i].person_list.end(), thread_cache[j].building_transfer[i].begin(), thread_cache[j].building_transfer[i].end());
                    size_t transfer_count = thread_cache[j].building_transfer[i].size();
                    if(is_rain) {
                        building[i].umbre_count += transfer_count;
                    }
                    thread_cache[thread_id].transfer_count += transfer_count;
                    thread_cache[j].building_transfer[i].clear();
                    thread_cache[j].building_transfer[i].shrink_to_fit();
                }
                building[i].person_list.shrink_to_fit();
            }
            #pragma omp single nowait private(i)
            {
                output.transfer_count = 0;
                output.starve_count = 0;
                output.lose_umbre_count = 0;
                for(i = 0; i < thread_count; ++i) {
                    output.transfer_count += thread_cache[i].transfer_count;
                    output.starve_count += thread_cache[i].starve_count;
                    output.lose_umbre_count += thread_cache[i].lose_umbre_count;

                    thread_cache[i].transfer_count = 0;
                    thread_cache[i].starve_count = 0;
                    thread_cache[i].lose_umbre_count = 0;
                }
            }
        }
        input.output_function(output);
    }
    delete[]building;
}

