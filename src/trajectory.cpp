#include "trajectory.hpp"

#include <iomanip>
#include <limits>
#include <locale>

namespace nbody {

TrajectoryWriter::TrajectoryWriter(const std::filesystem::path& directory)
{
    std::filesystem::create_directories(directory);
    file_.exceptions(std::ios::failbit | std::ios::badbit);
    file_.imbue(std::locale::classic());
    file_.open(directory / "trajectory.csv");
    file_ << std::setprecision(std::numeric_limits<double>::max_digits10);
    file_ << "step,time,particle_id,x,y,z\n";
}

void TrajectoryWriter::write_frame(std::size_t step, double time,
                                   const std::vector<Particle>& particles)
{
    for (std::size_t id = 0; id < particles.size(); ++id) {
        const auto& position = particles[id].position;
        file_ << step << ',' << time << ',' << id << ','
              << position.x << ',' << position.y << ',' << position.z << '\n';
    }
}

void TrajectoryWriter::close()
{
    file_.close();
}

} // namespace nbody
