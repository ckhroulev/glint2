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

#include <glint2/Grid_XY.hpp>
#include <glint2/clippers.hpp>
//#include <glint2/constants.hpp>
#include <boost/filesystem.hpp>

static const double km = 1000.0;

using namespace glint2;

int main(int argc, char **argv)
{
	printf("------------- Set up the local ice grid\n");

	Grid_XY grid;
	grid.name = "searise50";
	grid.sproj = "+proj=stere +lon_0=-39 +lat_0=90 +lat_ts=71.0 +ellps=WGS84";

	// Approximate 50km SeaRISE-like grid
	set_xy_boundaries(grid,
		(-800)*km, (-800 + 300*5)*km,     50*km,
		(-3400)*km, (-3400 + 560*5)*km,   50*km);

	grid.realize(boost::bind(&EuclidianClip::keep_all, _1));

	printf("Ice grid has %ld cells\n", grid.ncells_full());

	// ------------- Write it out to NetCDF
	fflush(stdout);
	printf("// ------------- Write it out to NetCDF\n");
	boost::filesystem::path exe_path(argv[0]);
	grid.to_netcdf(exe_path.stem().string() + ".nc");
//	grid.to_netcdf("searise.nc");
}
