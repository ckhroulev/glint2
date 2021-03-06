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

#include <giss/ncutil.hpp>

namespace giss {

NcDim *get_or_add_dim(NcFile &nc, std::string const &dim_name, long dim_size)
{
	// Look up dim the slow way...
	int num_dims = nc.num_dims();
	for (int i=0; i<num_dims; ++i) {
		NcDim *dim = nc.get_dim(i);
		char const *name = dim->name();

		if (strcmp(name, dim_name.c_str()) == 0) {
			long sz = dim->size();
			if (sz != dim_size) {
				fprintf(stderr, "Error: dimension %s (size = %ld) being redefined to size = %ld\n",
					dim_name.c_str(), sz, dim_size);
				throw std::exception();
			}
			return dim;
		}
	}

	return nc.add_dim(dim_name.c_str(), dim_size);
}

NcVar *get_var_safe(NcFile &nc, std::string const &var_name)
{
	// Look up var the slow way...
	int num_vars = nc.num_vars();
	for (int i=0; i<num_vars; ++i) {
		NcVar *var = nc.get_var(i);
		char const *name = var->name();

		if (strcmp(name, var_name.c_str()) == 0) {
			return var;
		}
	}
	return NULL;
}

#if 0
std::vector<double> read_double_vector(NcFile &nc, std::string const &var_name)
{
	// Read points vector
	NcVar *vpoints = nc.get_var(var_name.c_str());
	long npoints = vpoints->get_dim(0)->size();
	std::vector<double> points(npoints);
	vpoints->get(&points[0], npoints);
	return points;
}

std::vector<int> read_int_vector(NcFile &nc, std::string const &var_name)
{
	// Read points vector
	NcVar *vpoints = nc.get_var(var_name.c_str());
	long npoints = vpoints->get_dim(0)->size();
	std::vector<int> points(npoints);
	vpoints->get(&points[0], npoints);
	return points;
}
#endif

void netcdf_write_functions(std::vector<boost::function<void ()>> const &functions)
{
	for (auto fn = functions.begin(); fn != functions.end(); ++fn) (*fn)();
}

}
