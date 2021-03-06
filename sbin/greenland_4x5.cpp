/*
 * GLINT2: A Coupling Library for Ice Models and GCMs
 * Copyright (c) 2013 by Robert Fischer
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <boost/bind.hpp>
#include <glint2/Grid_LonLat.hpp>
#include <glint2/clippers.hpp>
#include <boost/filesystem.hpp>
#include <glint2/modele/grids_ll.hpp>
#include <glint2/clippers.hpp>

static const double km = 1000.0;

using namespace glint2;

const double EQ_RAD = 6.371e6; /// Radius of the Earth (same as in ModelE)
//const double EQ_RAD = 6370997; /// Radius of the Earth (same as in proj.4, see src/pj_ellps.c)

int main(int argc, char **argv)
{
	printf("------------- Set up GCM Grid\n");

	Grid_LonLat grid;
	glint2::modele::set_lonlat_4x5(grid);
	grid.name = "greenland_4x5";
	grid.points_in_side = 4;
	grid.eq_rad = EQ_RAD;
	grid.realize(boost::bind(
		&SphericalClip::lonlat, -74., 59., -10., 87.5,
		_1, _2, _3, _4));

//	grid.realize(boost::bind(&SphericalClip::keep_all, _1, _2, _3, _4));

	// ------------- Write it out to NetCDF
	fflush(stdout);
	printf("// ------------- Write it out to NetCDF\n");
	boost::filesystem::path exe_path(argv[0]);
	grid.to_netcdf(exe_path.stem().string() + ".nc");
//	grid.to_netcdf("greenland_4x5.nc");
}
