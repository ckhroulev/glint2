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
#include <giss/geodesy.hpp>
#include <glint2/Grid_LonLat.hpp>
//#include <glint2/clippers.hpp>
//#include <snowdrift/maputils.hpp>
#include <glint2/gridutil.hpp>
#include <giss/ncutil.hpp>
#include <giss/constant.hpp>

namespace glint2 {


template <typename T>
inline int sgn(T val) {
    return (val > T(0)) - (val < T(0));
}

// ---------------------------------------------------------
std::vector<double> Grid_LonLat::latc() const
{
	std::vector<double> ret;
	ret.reserve(nlat());

	if (south_pole) ret.push_back(-90);
	for (int j=0; j<latb.size()-1; ++j)
		ret.push_back(.5 * (latb[j] + latb[j+1]));
	if (north_pole) ret.push_back(90);

	return ret;
}

std::vector<double> Grid_LonLat::lonc() const
{
	std::vector<double> ret;
	ret.reserve(nlon());

	for (int j=0; j<lonb.size()-1; ++j)
		ret.push_back(.5 * (lonb[j] + lonb[j+1]));

	return ret;
}


// ---------------------------------------------------------

/** Computes the exact surface area of a lat-lon grid box on the
surface of the sphere.
<b>NOTES:</b>
<ol><li>lon0 must be numerically less than lon1.</li>
<li>lat0 and lat1 cannot cross the equator.</li></ol> */
inline double graticule_area_exact(Grid_LonLat const &grid,
	double lat0_deg, double lat1_deg,
	double lon0_deg, double lon1_deg)
{
	double delta_lon_deg = lon1_deg - lon0_deg;
	delta_lon_deg = loncorrect(delta_lon_deg, 0);

//printf("delta_lon_deg=%f\n", delta_lon_deg);

	double lat0 = lat0_deg * giss::D2R;
	double lat1 = lat1_deg * giss::D2R;
	double delta_lon = delta_lon_deg * giss::D2R;

//printf("lat1=%f, abs(lat1)=%f\n",lat1,std::abs(lat1));
//printf("%f %f %f %f %f %f\n", delta_lon, grid.eq_rad, lat1, lat0, sin(lat1), sin(lat0));
	return delta_lon * (grid.eq_rad*grid.eq_rad) * (sin(lat1) - sin(lat0));
}

/** The polar graticule is a (latitudinal) circle centered on the pole.
This computes its area. */
inline double polar_graticule_area_exact(Grid_LonLat const &grid,
	double radius_deg)
{
	// See http://en.wikipedia.org/wiki/Spherical_cap
	double theta = radius_deg * giss::D2R;
	return 2.0 * M_PI * (grid.eq_rad * grid.eq_rad) * (1.0 - cos(theta));
}

// ---------------------------------------------------------
/** Set up a Grid_LonLat with a given specification.
@param spherical_clip Only realize grid cells that pass this test (before projection).
@see EuclidianClip, SphericalClip
*/
void Grid_LonLat::realize(
	boost::function<bool(double, double, double, double)> const &spherical_clip)
//	boost::function<bool(Cell const &)> const &euclidian_clip
{
	// Error-check the input parameters
	if (south_pole && latb[0] == -90.0) {
		std::cerr << "latb[] cannot include -90.0 if you're including the south pole cap" << std::endl;
		throw std::exception();
	}
	if (south_pole && latb.back() == 90.0) {
		std::cerr << "latb[] cannot include 90.0 if you're including the north pole cap" << std::endl;
		throw std::exception();
	}

	clear();

	// Set up to project lines on sphere (and eliminate duplicate vertices)	
	VertexCache vcache(this);
//	LineProjector projector(proj, &vcache);
//	printf("Using projection: \"%s\"\n", projector.proj.get_def().c_str());
//	printf("Using lat-lon projection: \"%s\"\n", projector.llproj.get_def().c_str());

	// ------------------- Set up the GCM Grid
	const int south_pole_offset = (south_pole ? 1 : 0);
	const int north_pole_offset = (north_pole ? 1 : 0);

	_ncells_full = nlon() * nlat();
	_nvertices_full = -1;	// We don't care for L0 grid

//	_nlon = lonb.size() - 1;
//	_nlat = latb.size() - 1 + south_pole_offset + north_pole_offset;

	// Get a bunch of points.  (i,j) is gridcell's index in canonical grid
	for (int ilat=0; ilat < latb.size()-1; ++ilat) {
		double lat0 = latb[ilat];
		double lat1 = latb[ilat+1];

		for (int ilon=0; ilon< lonb.size()-1; ++ilon) {
			Cell cell;
			double lon0 = lonb[ilon];
			double lon1 = lonb[ilon+1];

//printf("(ilon, ilat) = (%d, %d)\n", ilon, ilat);
//printf("values = %f %f %f %f\n", lon0, lat0, lon1, lat1);

			if (!spherical_clip(lon0, lat0, lon1, lat1)) continue;

			// Project the grid cell boundary to a planar polygon
			int n = points_in_side;

			// Pre-compute our points so we use exact same ones each time.
			std::vector<double> lons;
			lons.reserve(n+1);
			for (int i=0; i<=n; ++i)
				lons.push_back(lon0 + (lon1-lon0) * ((double)i/(double)n));

			std::vector<double> lats;
			lats.reserve(n+1);
			for (int i=0; i<=n; ++i)
				lats.push_back(lat0 + (lat1-lat0) * ((double)i/(double)n));

			// Build a square out of them (in lon/lat space)
			for (int i=0; i<n; ++i)
				vcache.add_vertex(cell, lons[i], lat0);

			for (int i=0; i<n; ++i)
				vcache.add_vertex(cell, lon1, lats[i]);

			// Try to keep calculations EXACTLY the same for VertexCache
			for (int i=n; i>0; --i)
				vcache.add_vertex(cell, lons[i], lat1);

			for (int i=n; i>0; --i)
				vcache.add_vertex(cell, lon0, lats[i]);

			// Figure out how to number this grid cell
			cell.j = ilat + south_pole_offset;	// 0-based 2-D index
			cell.i = ilon;
			cell.index = (cell.j * nlon() + cell.i);
			cell.area = graticule_area_exact(*this, lat0,lat1,lon0,lon1);

//printf("Adding lon/lat cell %d (%d, %d) area=%f\n", cell.index, cell.i, cell.j, cell.area);
			add_cell(std::move(cell));
		}
	}

	// Make the polar caps (if this grid specifies them)

	// North Pole cap
	double lat = latb.back();
	if (north_pole && spherical_clip(0, lat, 360, 90)) {
		Cell pole;
		for (int ilon=0; ilon< lonb.size()-1; ++ilon) {
			double lon0 = lonb[ilon];
			double lon1 = lonb[ilon+1];

			int n = points_in_side;
			for (int i=0; i<n; ++i) {
				double lon = lon0 + (lon1-lon0) * ((double)i/(double)n);
				pole.add_vertex(vcache.add_vertex(lon, lat));
			}
		}

		pole.i = nlon()-1;
		pole.j = nlat();
		pole.index = (pole.j * nlon() + pole.i);
		pole.area = polar_graticule_area_exact(*this, 90.0 - lat);

		add_cell(std::move(pole));
	}

	// South Pole cap
	lat = latb[0];
	if (south_pole && spherical_clip(0, -90, 360, lat)) {
		Cell pole;
		for (int ilon=lonb.size()-1; ilon >= 1; --ilon) {
			double lon0 = lonb[ilon];		// Make the circle counter-clockwise
			double lon1 = lonb[ilon-1];

			int n = points_in_side;
			for (int i=0; i<n; ++i) {
				double lon = lon0 + (lon1-lon0) * ((double)i/(double)n);
				pole.add_vertex(vcache.add_vertex(lon, lat));
			}
		}
		pole.i = 0;
		pole.j = 0;
		pole.index = 0;
		pole.area = polar_graticule_area_exact(*this, 90.0 + lat);

		add_cell(std::move(pole));
	}
}

// ---------------------------------------------------------

static void Grid_LonLat_netcdf_write(
	boost::function<void()> const &parent,
	NcFile *nc, Grid_LonLat const *grid, std::string const &vname)
{
	parent();

	if (grid->lonb.size() > 0) {
		NcVar *lonb_var = nc->get_var((vname + ".lon_boundaries").c_str());
		lonb_var->put(&grid->lonb[0], grid->lonb.size());
	}

	if (grid->latb.size() > 0) {
		NcVar *latb_var = nc->get_var((vname + ".lat_boundaries").c_str());
		latb_var->put(&grid->latb[0], grid->latb.size());
	}
}

boost::function<void ()> Grid_LonLat::netcdf_define(NcFile &nc, std::string const &vname) const
{
	auto parent = Grid::netcdf_define(nc, vname);

	NcDim *lonbDim = nc.add_dim((vname + ".lon_boundaries.length").c_str(),
		this->lonb.size());
	NcVar *lonb_var = nc.add_var((vname + ".lon_boundaries").c_str(),
		ncDouble, lonbDim);

	NcDim *latbDim = nc.add_dim((vname + ".lat_boundaries.length").c_str(),
		this->latb.size());
	NcVar *latb_var = nc.add_var((vname + ".lat_boundaries").c_str(),
		ncDouble, latbDim);

	NcVar *info_var = nc.get_var((vname + ".info").c_str());
	info_var->add_att("north_pole_cap", north_pole ? 1 : 0);
	info_var->add_att("south_pole_cap", south_pole ? 1 : 0);
	info_var->add_att("points_in_side", points_in_side);
	info_var->add_att("nlon", nlon());
	info_var->add_att("nlat", nlat());

	return boost::bind(&Grid_LonLat_netcdf_write, parent, &nc, this, vname);
}
// ---------------------------------------------------------

void Grid_LonLat::read_from_netcdf(NcFile &nc, std::string const &vname)
{
	Grid::read_from_netcdf(nc, vname);

	NcVar *info_var = nc.get_var((vname + ".info").c_str());
	north_pole = (giss::get_att(info_var, "north_pole_cap")->as_int(0) != 0);
	south_pole = (giss::get_att(info_var, "south_pole_cap")->as_int(0) != 0);
	points_in_side = giss::get_att(info_var, "points_in_side")->as_int(0);
//	_nlon = giss::get_att(info_var, "nlon")->as_int(0);
//	_nlat = giss::get_att(info_var, "nlat")->as_int(0);

	lonb = giss::read_double_vector(nc, vname + ".lon_boundaries");
	latb = giss::read_double_vector(nc, vname + ".lat_boundaries");
}


}
