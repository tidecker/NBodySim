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

std::vector<Frame> load_trajectory(const std::filesystem::path& path, const LoadProgress& progress)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open trajectory: " + path.string());
    }
    std::error_code size_error;
    const auto size = progress ? std::filesystem::file_size(path, size_error) : 0;
    std::string line;
    read_line(input, line);
    const bool has_color = line == "step,time,particle_id,x,y,z,radius,red,green,blue";
    const bool has_radius = has_color || line == "step,time,particle_id,x,y,z,radius";
    if (!has_radius && line != "step,time,particle_id,x,y,z") {
        throw std::runtime_error("Expected CSV header step,time,particle_id,x,y,z[,radius[,red,green,blue]]");
    }

    std::vector<Frame> frames;
    std::size_t line_number = 1;
    while (read_line(input, line)) {
        ++line_number;
        if (progress && line_number % 2048 == 0) {
            const auto position = input.tellg();
            progress(!size_error && size > 0 && position >= 0
                ? static_cast<double>(position) / size : 0);
        }
        try {
            std::array<std::string, 10> fields;
            const std::size_t field_count = has_color ? 10 : has_radius ? 7 : 6;
            std::size_t offset = 0;
            for (std::size_t i = 0; i < field_count; ++i) {
                const auto comma = line.find(',', offset);
                if ((comma == std::string::npos) != (i + 1 == field_count)) {
                    throw std::runtime_error("unexpected number of fields");
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
            const float radius = has_radius ? parse_number<float>(fields[6]) : 0;
            if (has_radius && radius <= 0) {
                throw std::runtime_error("radius must be positive");
            }

            if (step == frames.size()) {
                if (frames.empty() ? time != 0 : time <= frames.back().time) {
                    throw std::runtime_error("frame times must start at zero and increase");
                }
                frames.push_back({time, {}});
            }
            if (frames.empty() || step != frames.size() - 1 || time != frames.back().time) {
                throw std::runtime_error("expected contiguous steps with one time per frame");
            }
            auto& bodies = frames.back().bodies;
            if (!bodies.empty() && id <= bodies.back().id) {
                throw std::runtime_error("particle IDs must be unique and increasing in each frame");
            }
            Body body{position, radius, id, std::nullopt};
            if (has_color && !(fields[7].empty() && fields[8].empty() && fields[9].empty())) {
                body.color.emplace();
                for (std::size_t channel = 0; channel < body.color->size(); ++channel) {
                    const int value = parse_number<int>(fields[7 + channel]);
                    if (value < 0 || value > 255) {
                        throw std::runtime_error("color channels must be between 0 and 255");
                    }
                    (*body.color)[channel] = static_cast<float>(value) / 255;
                }
            }
            bodies.push_back(body);
        } catch (const std::runtime_error& error) {
            throw std::runtime_error("CSV line " + std::to_string(line_number) + ": " + error.what());
        }
    }
    if (input.bad() || !input.eof()) {
        throw std::runtime_error("Error reading trajectory: " + path.string());
    }
    input.clear(); // EOF is expected. Check close independently of the final read.
    input.close();
    if (!input) {
        throw std::runtime_error("Error closing trajectory: " + path.string());
    }
    if (frames.empty()) {
        throw std::runtime_error("Trajectory contains no frames");
    }
    if (progress) {
        progress(1);
    }
    return frames;
}

} // namespace viewer
