#include "csv.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>

namespace viewer {
namespace {

std::size_t parse_index(const std::string& field)
{
    std::size_t value{};
    const auto end = field.data() + field.size();
    const auto result = std::from_chars(field.data(), end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw std::runtime_error("expected a nonnegative step or particle ID");
    }
    return value;
}

template<typename Number>
Number parse_number(const std::string& field)
{
    std::istringstream input(field);
    input.imbue(std::locale::classic());
    Number value{};
    if (!(input >> std::noskipws >> value) || !std::isfinite(value) ||
        input.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("expected a finite numeric field");
    }
    return value;
}

bool read_line(std::istream& input, std::string& line)
{
    if (!std::getline(input, line)) {
        return false;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return true;
}

} // namespace

std::vector<Frame> load_trajectory(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot open trajectory: " + path.string());
    }
    std::string line;
    if (!read_line(input, line) || line != "step,time,particle_id,x,y,z") {
        throw std::runtime_error("Expected CSV header step,time,particle_id,x,y,z");
    }

    std::vector<Frame> frames;
    std::size_t line_number = 1;
    while (read_line(input, line)) {
        ++line_number;
        try {
            std::array<std::string, 6> fields;
            std::size_t offset = 0;
            for (std::size_t i = 0; i < fields.size(); ++i) {
                const auto comma = line.find(',', offset);
                if ((comma == std::string::npos) != (i + 1 == fields.size())) {
                    throw std::runtime_error("expected six fields");
                }
                fields[i] = line.substr(offset, comma - offset);
                offset = comma + 1;
            }

            const auto step = parse_index(fields[0]);
            const auto time = parse_number<double>(fields[1]);
            const auto id = parse_index(fields[2]);
            const Position position{parse_number<float>(fields[3]),
                                    parse_number<float>(fields[4]),
                                    parse_number<float>(fields[5])};

            if (step == frames.size()) {
                if (frames.empty() ? time != 0 : time <= frames.back().time) {
                    throw std::runtime_error("frame times must start at zero and increase");
                }
                if (frames.size() > 1 &&
                    frames.back().positions.size() != frames.front().positions.size()) {
                    throw std::runtime_error("particle count changed between frames");
                }
                frames.push_back({time, {}});
            }
            if (frames.empty() || step != frames.size() - 1 || time != frames.back().time ||
                id != frames.back().positions.size()) {
                throw std::runtime_error("expected contiguous steps and particle IDs with one time per frame");
            }
            frames.back().positions.push_back(position);
        } catch (const std::runtime_error& error) {
            throw std::runtime_error("CSV line " + std::to_string(line_number) + ": " + error.what());
        }
    }
    if (input.bad()) {
        throw std::runtime_error("Error reading trajectory: " + path.string());
    }
    if (frames.empty()) {
        throw std::runtime_error("Trajectory contains no frames");
    }
    if (frames.back().positions.size() != frames.front().positions.size()) {
        throw std::runtime_error("Particle count changed in the last frame");
    }
    return frames;
}

} // namespace viewer
