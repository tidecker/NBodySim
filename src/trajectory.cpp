#include "trajectory.hpp"

#include <iomanip>
#include <limits>
#include <locale>

namespace nbody {

TrajectoryWriter::TrajectoryWriter(const std::filesystem::path& directory, bool write_color)
    : write_color_(write_color)
{
    std::filesystem::create_directories(directory);
    file_.exceptions(std::ios::failbit | std::ios::badbit);
    file_.imbue(std::locale::classic());
    file_.open(directory / "trajectory.csv");
    file_ << std::setprecision(std::numeric_limits<double>::max_digits10);
    file_ << "step,time,particle_id,x,y,z,radius";
    if (write_color_) {
        file_ << ",red,green,blue";
    }
    file_ << '\n';
}

void TrajectoryWriter::write_frame(std::size_t step, double time,
                                   const std::vector<Particle>& particles)
{
    for (const auto& particle : particles) {
        const auto& position = particle.position;
        file_ << step << ',' << time << ',' << particle.id << ','
              << position.x << ',' << position.y << ',' << position.z << ',' << particle.radius;
        if (write_color_) {
            file_ << ',' << particle.color[0] << ',' << particle.color[1] << ',' << particle.color[2];
        }
        file_ << '\n';
    }
}

void TrajectoryWriter::close()
{
    file_.close();
}

} // namespace nbody
