#include "csv.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

int main()
{
    try {
        {
            std::ofstream out("legacy-trajectory-test.csv");
            out << "step,time,particle_id,x,y,z\n0,0,0,1,2,3\n1,1,0,4,5,6\n";
        }
        const auto legacy = viewer::load_trajectory("legacy-trajectory-test.csv");
        if (legacy.size() != 2 || legacy.back().bodies.front().radius != 0 ||
            legacy.back().bodies.front().position[2] != 6 || legacy.back().bodies.front().color) {
            throw std::runtime_error("Could not read legacy trajectory");
        }
        {
            std::ofstream out("merge-trajectory-test.csv");
            out << "step,time,particle_id,x,y,z,radius\n"
                   "0,0,0,1,2,3,0.125\n0,0,1,2,3,4,0.125\n"
                   "1,0.5,2,2,3,4,0.25\n2,1,2,3,4,5,0.25\n";
        }
        const auto modern = viewer::load_trajectory("merge-trajectory-test.csv");
        if (modern.size() != 3 || modern[0].bodies.size() != 2 || modern[1].bodies.size() != 1 ||
            modern[1].bodies[0].id != 2 || modern[2].bodies[0].id != 2 ||
            modern[1].bodies[0].radius != 0.25f) {
            throw std::runtime_error("Could not read variable-size trajectory with stable IDs");
        }
        {
            std::ofstream out("color-trajectory-test.csv");
            out << "step,time,particle_id,x,y,z,radius,red,green,blue\n"
                   "0,0,0,1,2,3,0.25,0,0,0\n0,0,1,0,0,0,0.5,255,128,0\n"
                   "0,0,2,4,5,6,0.25,,,\n"
                   "1,1,1,0,0,0,0.625,255,128,0\n";
        }
        bool closed_at_completion = false;
        const auto colored = viewer::load_trajectory("color-trajectory-test.csv", [&](double fraction) {
            if (fraction == 1) {
                std::filesystem::rename("color-trajectory-test.csv", "color-trajectory-renamed.csv");
                std::filesystem::rename("color-trajectory-renamed.csv", "color-trajectory-test.csv");
                closed_at_completion = true;
            }
        });
        const std::array<float, 3> black{0, 0, 0};
        const std::array<float, 3> orange{1, 128.0f / 255, 0};
        if (!closed_at_completion || colored.size() != 2 ||
            colored[0].bodies.size() != 3 || colored[1].bodies.size() != 1 ||
            colored[0].bodies[0].color != black || colored[0].bodies[1].color != orange ||
            colored[0].bodies[2].color ||
            colored[1].bodies[0].id != 1 || colored[1].bodies[0].color != orange ||
            colored[1].bodies[0].radius != 0.625f) {
            throw std::runtime_error("Could not read trajectory with custom RGB");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
