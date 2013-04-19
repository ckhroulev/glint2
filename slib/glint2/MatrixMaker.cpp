#include <giss/CooVector.hpp>
#include <giss/ncutil.hpp>
#include <glint2/MatrixMaker.hpp>
#include <glint2/IceSheet_L0.hpp>
#include <glint2/HCIndex.hpp>

namespace glint2 {

void MatrixMaker::clear()
{
	sheets.clear();
	sheets_by_id.clear();
	grid1.reset();
	mask1.reset();
	hpdefs.clear();
	// hcmax.clear();
}

void MatrixMaker::realize() {

	// ---------- Check array bounds
	long n1 = grid1->ndata();
	if (mask1.get() && mask1->extent(0) != n1) {
		fprintf(stderr, "mask1 for %s has wrong size: %d (vs %d expected)\n",
			mask1->extent(0), n1);
		throw std::exception();
	}

	long nhc = hpdefs.size();
	if (hcmax.extent(0) != nhc) {
		fprintf(stderr, "hcmax for %s has wrong size: %d (vs %d expected)\n",
			mask1->extent(0), n1);
		throw std::exception();
	}

	// ------------- Realize the ice sheets
	for (auto sheet=sheets.begin(); sheet != sheets.end(); ++sheet)
		sheet->realize();
}

int MatrixMaker::add_ice_sheet(std::unique_ptr<IceSheet> &&sheet)
{
	if (sheet->name == "") {
		fprintf(stderr, "MatrixMaker::add_ice_sheet(): Sheet must have a name\n");
		throw std::exception();
	}

	int const index = _next_sheet_index++;
	sheet->index = index;
printf("MatrixMaker: %p.sheetno = %d\n", &*sheet, sheet->index);
	sheet->gcm = this;
	
	sheets_by_id.insert(std::make_pair(sheet->index, sheet.get()));
	sheets.insert(sheet->name, std::move(sheet));
	return index;
}

#if 0
// NOT fully filtered.
std::vector<int> MatrixMaker::get_used1()
{
	std::unordered_set<int> used;
	for (auto sheet = sheets.begin(); sheet != sheets.end(); ++sheet) {
//		sheet->accum_used(used, domain.get());
		sheet->accum_used(used);
	}

	// Return sorted vector
	std::vector<int> ret;
	ret.reserve(used.size());
	for (auto ii = used.begin(); ii != used.end(); ++ii) ret.push_back(*ii);
	std::sort(ret.begin(), ret.end());

	return ret;
}
#endif


/** NOTE: Does not necessarily assume that ice sheets do not overlap on the same GCM grid cell */
void MatrixMaker::compute_fgice(
	giss::CooVector<int,double> &fgice1)
{

	// Accumulate areas over all ice sheets
	giss::SparseAccumulator<int,double> area1_m_hc;
	fgice1.clear();
	for (auto sheet = sheets.begin(); sheet != sheets.end(); ++sheet) {

		// Local area1_m just for this ice sheet
		giss::SparseAccumulator<int,double> larea1_m;
		sheet->accum_areas(larea1_m, area1_m_hc);

		// Use the local area1_m to contribute to fgice1
		giss::Proj2 proj;
		grid1->get_ll_to_xy(proj, sheet->grid2->sproj);
		for (auto ii = larea1_m.begin(); ii != larea1_m.end(); ++ii) {
			int const i1 = ii->first;
			double ice_covered_area = ii->second;
			Cell *cell = grid1->get_cell(i1);
			double area1 = area_of_proj_polygon(*cell, proj);
//printf("area1(%d) = %f\n", i1, area1);
			fgice1.add(i1, ice_covered_area / area1);

		}
	}
	fgice1.sort();


	printf("END compute_fgice()\n");
}

/** NOTE: Does not necessarily assume that ice sheets do not overlap on the same GCM grid cell */
void MatrixMaker::compute_fhc(
	giss::CooVector<std::pair<int,int>,double> &fhc1h)	// std::pair<i1, hc>
{

	// Accumulate areas over all ice sheets
	giss::SparseAccumulator<int,double> area1_m;
	giss::SparseAccumulator<int,double> area1_m_hc;
	for (auto sheet = sheets.begin(); sheet != sheets.end(); ++sheet) {
		sheet->accum_areas(area1_m, area1_m_hc);
	}

	// Summing duplicates on area1_m and area1_m_hc not needed
	// because the unordered_map sums them automatically.

	// Compute fhc1h.  Unlike fgice1, this does NOT need to be done
	// separately for each ice sheet.
	fhc1h.clear();
	HCIndex hc_index(n1());
	for (auto ii = area1_m_hc.begin(); ii != area1_m_hc.end(); ++ii) {
		int i1hc = ii->first;

		// Separate out into grid cell and height class
		int i1, hc;
		hc_index.index_to_ik(i1hc, i1, hc);

		fhc1h.add(std::make_pair(i1, hc), ii->second / area1_m[i1]);
	}
	fhc1h.sort();

	printf("END compute_fhc()\n");
}




/** TODO: This doesn't account for spherical earth */
std::unique_ptr<giss::VectorSparseMatrix> MatrixMaker::hp_to_hc()
{
	int n1_nhc = grid1->ndata() * nhc();
	std::unique_ptr<giss::VectorSparseMatrix> ret(
		new giss::VectorSparseMatrix(
		giss::SparseDescr(n1_nhc, n1_nhc)));

	// Compute the hp->ice and ice->hc transformations for each ice sheet
	// and combine into one hp->hc matrix for all ice sheets.
	giss::SparseAccumulator<int,double> area1_m_hc;
	for (auto sheet = sheets.begin(); sheet != sheets.end(); ++sheet) {
printf("***** sheet: %s\n", sheet->name.c_str());
		giss::VectorSparseMatrix &hp_to_ice = sheet->hp_to_ice();
		auto ice_to_hc(sheet->ice_to_hc(area1_m_hc));

#if 0
std::vector<boost::function<void ()>> fns;
giss::SparseAccumulator<int,double> area1_m_hc_inv;
NcFile nc("i2hc.nc", NcFile::Replace);
divide_by(*ice_to_hc, area1_m_hc, area1_m_hc_inv);
fns.push_back(hp_to_ice.netcdf_define(nc, "hp2i"));
fns.push_back(ice_to_hc->netcdf_define(nc, "i2hc"));
for (auto ii=fns.begin(); ii != fns.end(); ++ii) (*ii)();
nc.close();
#endif

		ret->append(*multiply(*ice_to_hc, hp_to_ice));
	}

	giss::SparseAccumulator<int,double> area1_m_hc_inv;
	divide_by(*ret, area1_m_hc, area1_m_hc_inv);
printf("After divide_by: %ld %d\n", area1_m_hc.size(), area1_m_hc_inv.size());
	ret->sum_duplicates();

printf("Writing hp2hc ret = %p\n", ret.get());
NcFile nc("hp2hc.nc", NcFile::Replace);
ret->netcdf_define(nc, "hp2hc")();
nc.close();
printf("Done Writing hp2hc ret = %p\n", ret.get());

	return ret;
}
// --------------------------------------------------------------
/** @return fhpmat[i, (j, hp)]: the amount of area that the height
           point hp of cell j contributes to cell i in height-class
           space. */
std::unique_ptr<giss::VectorSparseMatrix>  MatrixMaker::compute_fhpmat(
	giss::VectorSparseMatrix const &hp_to_hc,
	SparseAccumulator1hc const &fhc1h) const
//	giss::SparseAccumulator<std::pair<int,int>, double> const &fhc1h) const
{
	int const n1 = grid1->ndata();
	std::unique_ptr<giss::VectorSparseMatrix> fhpmat(
		new giss::VectorSparseMatrix(giss::SparseDescr(n1, n1*nhp())));

	HCIndex hc_index(n1);
	for (auto ii = hp_to_hc.begin(); ii != hp_to_hc.end(); ++ii) {
		// Separate out into grid cell and height point / class
		int i0, hp0;		// Columns (domain of linear transformation)
		hc_index.index_to_ik(ii.col(), i0, hp0);
		int i1, hc1;		// Rows (range)
		hc_index.index_to_ik(ii.row(), i1, hc1);

		// See if there IS an fhc number for this
		auto fhc_ptr(fhc1h.find(std::pair<int,int>(i1, hc1)));
		if (fhc_ptr == fhc1h.end()) continue;

		// Collapse down height classes, and multiply by fractional area of each
		fhpmat->add(i1, ii.col(), ii.val() * fhc_ptr->second);
	}
	fhpmat->sum_duplicates(giss::SparseMatrix::SortOrder::ROW_MAJOR);
	return fhpmat;
}
// --------------------------------------------------------------

/** Computes the relative contribution of one height point to
the total ice-covered area of a GCM cell, and the total area of a GCM cell,
respectively.  These are APPROXIMATE, since ice grid cells that overlap other
GCM cells are IGNORED.

@param hp_to_hc MUST BE SORTED ROW_MAJOR!!!
@return Indexed by (n1, nhc)
*/
giss::CooVector<std::pair<int,int>,double> MatrixMaker::compute_fhp_approx(
	giss::VectorSparseMatrix const &hp_to_hc,
	giss::VectorSparseMatrix const &fhpmat)
{
	giss::CooVector<std::pair<int,int>,double> fhp_approx;

	HCIndex hc_index(n1());
	auto rbegin(get_row_beginnings(fhpmat));

	// For each row of matrix
	for (int ri=0; ri<rbegin.size()-1; ++ri) {
		double sum_row = 0;	
		double sum_inrow = 0;

		// Get sums of this row
		for (int i=rbegin[ri]; i<rbegin[i+1]; ++i) {
			int const row = hp_to_hc.rows()[i];
			int const col = hp_to_hc.cols()[i];
			double const val = hp_to_hc.vals()[i];

			int i0, hp0;		// Columns (domain of linear transformation)
			hc_index.index_to_ik(hp_to_hc.cols()[i], i0, hp0);
			int const i1 = row;

			sum_row += val;
			if (i0 == i1) sum_inrow += val;
		}

		// Scale for portions of this row outside the gridcell
		double scale_factor = sum_row / sum_inrow;
		for (int i=rbegin[ri]; i<rbegin[i+1]; ++i) {
			int const row = hp_to_hc.rows()[i];
			int const col = hp_to_hc.cols()[i];
			double const val = hp_to_hc.vals()[i];

			int i0, hp0;		// Columns (domain of linear transformation)
			hc_index.index_to_ik(hp_to_hc.cols()[i], i0, hp0);
			int const i1 = row;

			if (i0 == i1) fhp_approx.add(
				std::make_pair(i0,hp0),   val * scale_factor);
		}
	}

	return fhp_approx;
}
// --------------------------------------------------------------
// --------------------------------------------------------------
// ==============================================================
// Write out the parts that this class computed --- so we can test/check them

boost::function<void ()> MatrixMaker::netcdf_define(NcFile &nc, std::string const &vname) const
{
	std::vector<boost::function<void ()>> fns;
	fns.reserve(sheets.size() + 1);

printf("MatrixMaker::netcdf_define(%s) (BEGIN)\n", vname.c_str());

	// ------ Attributes
	auto one_dim = giss::get_or_add_dim(nc, "one", 1);
	NcVar *info_var = nc.add_var((vname + ".info").c_str(), ncInt, one_dim);

	// Names of the ice sheets
	std::string sheet_names = "";
	for (auto sheet = sheets.begin(); ; ) {
		sheet_names.append(sheet->name);
		++sheet;
		if (sheet == sheets.end()) break;
		sheet_names.append(",");
	}
	info_var->add_att("sheetnames", sheet_names.c_str());
#if 0
		info_var->add_att("grid1.name", gcm->grid1->name.c_str());
		info_var->add_att("grid2.name", grid2->name.c_str());
		info_var->add_att("exgrid.name", exgrid->name.c_str());
#endif

	// Define the variables
	fns.push_back(grid1->netcdf_define(nc, vname + ".grid1"));
	if (mask1.get())
		fns.push_back(giss::netcdf_define(nc, vname + "mask1", *mask1));
	fns.push_back(giss::netcdf_define(nc, vname + ".hpdefs", hpdefs));
	fns.push_back(giss::netcdf_define(nc, vname + ".hcmax", hcmax));
	for (auto sheet = sheets.begin(); sheet != sheets.end(); ++sheet) {
		fns.push_back(sheet->netcdf_define(nc, vname + "." + sheet->name));
	}


printf("MatrixMaker::netcdf_define(%s) (END)\n", vname.c_str());

	return boost::bind(&giss::netcdf_write_functions, fns);
}
// -------------------------------------------------------------
static std::vector<std::string> parse_comma_list(std::string list)
{
	std::stringstream ss(list);
	std::vector<std::string> result;

	while( ss.good() ) {
		std::string substr;
		getline( ss, substr, ',' );
		result.push_back( substr );
	}
	return result;
}

std::unique_ptr<IceSheet> read_icesheet(NcFile &nc, std::string const &vname)
{
	auto info_var = nc.get_var((vname + ".info").c_str());
	std::string stype(giss::get_att(info_var, "parameterization")->as_string(0));

	std::unique_ptr<IceSheet> sheet;
	if (stype == "L0") {
		sheet.reset(new IceSheet_L0);
	}
#if 0
	else if (stype == "L1") {
		sheet.reset(new IceSheet_L1);
	}
#endif

	sheet->read_from_netcdf(nc, vname);
	printf("read_icesheet(%s) END\n", vname.c_str());
	return sheet;

}


void MatrixMaker::read_from_netcdf(NcFile &nc, std::string const &vname)
{
	clear();

	printf("MatrixMaker::read_from_netcdf(%s) 1\n", vname.c_str());
	grid1.reset(read_grid(nc, vname + ".grid1").release());
	if (giss::get_var_safe(nc, vname + ".mask1")) {
		mask1.reset(new blitz::Array<int,1>(
		giss::read_blitz<int,1>(nc, vname + ".mask1")));
	}
	hpdefs = giss::read_vector<double>(nc, vname + ".hpdefs");
	hcmax.reference(giss::read_blitz<double,1>(nc, vname + ".hcmax"));

	printf("MatrixMaker::read_from_netcdf(%s) 2\n", vname.c_str());

//	grid2.reset(read_grid(nc, "grid2").release());
//	exgrid.reset(read_grid(nc, "exgrid").release());

	// Read list of ice sheets
	NcVar *info_var = nc.get_var((vname + ".info").c_str());
	std::vector<std::string> sheet_names = parse_comma_list(std::string(
		giss::get_att(info_var, "sheetnames")->as_string(0)));

	for (auto sname = sheet_names.begin(); sname != sheet_names.end(); ++sname) {
		std::string var_name(vname + "." + *sname);
		printf("MatrixMaker::read_from_netcdf(%s) %s 3\n",
			vname.c_str(), var_name.c_str());
		add_ice_sheet(read_icesheet(nc, var_name));
	}

	// Remove grid cells that are not part of this domain.
	// TODO: This should be done while reading the cells in the first place.
	boost::function<bool (int)> include_cell1(domain->get_in_halo2());
	grid1->filter_cells(include_cell1);

	// Now remove cells from the exgrids and grid2s that interacted with grid1
	for (auto sheet=sheets.begin(); sheet != sheets.end(); ++sheet) {
		sheet->filter_cells1(include_cell1);
	}

}

std::unique_ptr<IceSheet> new_ice_sheet(Grid::Parameterization parameterization)
{
	switch(parameterization.index()) {
		case Grid::Parameterization::L0 : {
			IceSheet *ics = new IceSheet_L0;
			return std::unique_ptr<IceSheet>(ics);
//			return std::unique_ptr<IceSheet>(new IceSheet_L0);
		} break;
#if 0
		case Grid::Parameterization::L1 :
			return std::unique_ptr<IceSheet>(new IceSheet_L1);
		break;
#endif
		default :
			fprintf(stderr, "Unrecognized parameterization: %s\n", parameterization.str());
			throw std::exception();
	}
}


}
