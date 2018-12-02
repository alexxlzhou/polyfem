#include <polyfem/Mesh2D.hpp>
#include <polyfem/Mesh3D.hpp>
#include <polyfem/Common.hpp>

#include <CLI/CLI.hpp>

#include <geogram/basic/command_line.h>
#include <geogram/basic/command_line_args.h>

#include <igl/opengl/glfw/imgui/ImGuiMenu.h>
#include <igl/opengl/glfw/imgui/ImGuiHelpers.h>
#include <igl/adjacency_list.h>
#include <igl/triangle_triangle_adjacency.h>
#include <igl/per_face_normals.h>
#include <imgui/imgui.h>

#include <igl/unproject_onto_mesh.h>
#include <igl/opengl/glfw/Viewer.h>
#include <igl/colormap.h>
#include <igl/file_dialog_open.h>
#include <igl/file_dialog_save.h>
#include <iostream>
#include <queue>
#include <vector>


using namespace polyfem;
using namespace Eigen;
static const int BUF_SIZE = 128;

igl::ColorMapType color_map = igl::COLOR_MAP_TYPE_VIRIDIS;

static const ImGuiWindowFlags WINDOW_FLAGS = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;

struct BCVals
{
	std::vector<std::array<float,3>> vals;
	std::vector<Matrix<std::array<char,BUF_SIZE>, 3, 1>> funs;
	std::vector<int> bc_type;
	std::vector<int> bc_value;

	void reset()
	{
		vals.clear();
		funs.clear();
		bc_type.clear();
		bc_value.clear();

		append();
	}

	void append()
	{
		vals.push_back(std::array<float,3>());
		vals.back()[0]=vals.back()[1]=vals.back()[2] = 0;
		Matrix<std::array<char,BUF_SIZE>, 3, 1> tmp;
		for(int i = 0; i < 3; ++i)
		{
			for(int j = 0; j < BUF_SIZE; ++j)
				tmp(i)[j] = 0;
		}
		funs.push_back(tmp);

		bc_type.push_back(0);
		bc_value.push_back(0);
	}

	int size() const
	{
		return vals.size();
	}
};

RowVector3d color(int bc, int n_cols)
{
	RowVector3d col;
	if(bc == 0)
	{
		col << 1,1,1;
	}
	else
	{
		MatrixXd tmp;
		MatrixXd v(1,1); v(0) = bc;
		igl::colormap(color_map, v, 0, n_cols, tmp);
		col = tmp;
	}

	return col;
}

bool load(const std::string &path, igl::opengl::glfw::Viewer &viewer,
	MatrixXd &V, MatrixXi &F, MatrixXd &p0, MatrixXd &p1, MatrixXd &N, MatrixXi &adj,
	VectorXi &selected, Matrix<std::vector<int>, Dynamic, 1> &all_2_local, VectorXi &boundary_2_all, MatrixXd &C)
{
	V.resize(0,0);
	F.resize(0,0);
	p0.resize(0,0);
	p1.resize(0,0);
	N.resize(0,0);
	adj.resize(0,0);
	selected.resize(0);

	all_2_local.resize(0);
	boundary_2_all.resize(0);

	C.resize(0, 0);

	auto tmp = Mesh::create(path, false);
	if (!tmp) {
		return false;
	}

	viewer.core.lighting_factor = (tmp->is_volume() ? 1.f : 0.f);

	std::cout<<"N vertices "<<tmp->n_vertices()<<std::endl;
	if(tmp->is_volume())
	{
		viewer.core.set_rotation_type(igl::opengl::ViewerCore::RotationType::ROTATION_TYPE_TRACKBALL);

		Mesh3D &mesh = *dynamic_cast<Mesh3D *>(tmp.get());

		std::vector<int> ranges;
		mesh.get_edges(p0, p1);
		V.resize(mesh.n_vertices() + mesh.n_faces(), 3);
		for(int i = 0; i < mesh.n_vertices(); ++i)
			V.row(i) = mesh.point(i);

		int v_index = mesh.n_vertices();

		F.resize(mesh.n_faces()*4, 3);
		boundary_2_all.resize(mesh.n_faces()*4);
		all_2_local.resize(mesh.n_faces());

		int index = 0;
		for(int f = 0; f < mesh.n_faces(); ++f)
		{
			if(!mesh.is_boundary_face(f))
				continue;

			std::vector<int> &other_faces = all_2_local(f);

			const int n_f_v = mesh.n_face_vertices(f);
			if(n_f_v == 3)
			{
				F.row(index) << mesh.face_vertex(f, 2), mesh.face_vertex(f, 1), mesh.face_vertex(f, 0);
				boundary_2_all(index) = f;
				other_faces.push_back(index);
				++index;
			}
			else
			{
				auto bary = mesh.face_barycenter(f);
				for(int j = 0; j < n_f_v; ++j)
				{
					F.row(index) << mesh.face_vertex(f, j), mesh.face_vertex(f, (j+1)%n_f_v), v_index;
					boundary_2_all(index) = f;
					other_faces.push_back(index);
					++index;
				}

				V.row(v_index) = bary;
				++v_index;
			}
		}

		F.conservativeResize(index, 3);
		V.conservativeResize(v_index, 3);
		boundary_2_all.conservativeResize(index);

		igl::triangle_triangle_adjacency(F, adj);
		igl::per_face_normals(V, F, N);


		// Initialize white
		C = MatrixXd::Constant(F.rows(),3,1);
		selected.resize(mesh.n_faces());
		selected.setZero();
	}
	else
	{
		viewer.core.set_rotation_type(igl::opengl::ViewerCore::RotationType::ROTATION_TYPE_NO_ROTATION);

		Mesh2D &mesh = *dynamic_cast<Mesh2D *>(tmp.get());

		std::vector<int> ranges;
		mesh.triangulate_faces(F,V, ranges);
		V.conservativeResize(V.rows(), 3); V.col(2).setZero();

		boundary_2_all.resize(mesh.n_edges()*2);
		all_2_local.resize(mesh.n_edges());


		int index = 0;

		p0.resize(mesh.n_edges(), 3);
		p1.resize(mesh.n_edges(), 3);
		for(int e = 0; e < mesh.n_edges(); ++e)
		{
			if(!mesh.is_boundary_edge(e))
				continue;

			std::vector<int> &other_edges = all_2_local(e);

			auto tmp0 = mesh.point(mesh.edge_vertex(e, 0));
			auto tmp1 = mesh.point(mesh.edge_vertex(e, 1));

			p0.row(index) << tmp0(0), tmp0(1), 0;
			p1.row(index) << tmp1(0), tmp1(1), 0;

			boundary_2_all(index) = e;
			other_edges.push_back(mesh.edge_vertex(e, 0));
			other_edges.push_back(mesh.edge_vertex(e, 1));
			other_edges.push_back(index);
			++index;
		}

		p0.conservativeResize(index, 3);
		p1.conservativeResize(index, 3);


		C = MatrixXd::Constant(p0.rows(),3,0);
		selected.resize(mesh.n_edges());
		selected.setZero();
	}

	return tmp->is_volume();
}

void save(const std::string &path, const VectorXi &selected, const BCVals &vals)
{
	std::ofstream file;
	file.open(path + ".txt");

	if(file.good())
	{
		file << selected;
	}

	file.close();

	auto dirichel = json::array();
	auto neuman = json::array();

	for(int i = 1; i <= vals.size(); ++i){
		json vv;
		if(vals.bc_value[i-1] == 0)
			vv = {{"id", i}, {"value", {vals.vals[i-1][0], vals.vals[i-1][1], vals.vals[i-1][2]}}};
		else
		{
			const std::string str0 = vals.funs[i-1][0].data();
			const std::string str1 = vals.funs[i-1][1].data();
			const std::string str2 = vals.funs[i-1][2].data();
			vv = {{"id", i}, {"value", {str0, str1, str2}}};
		}

		if(vals.bc_type[i-1] == 0)
			dirichel.push_back(vv);
		else
			neuman.push_back(vv);
	}
	const json args = {
		{"dirichlet_boundary", dirichel},
		{"neumann_boundary", neuman},
	};

	file.open(path + ".json");
	file << args.dump(4) << std::endl;
	file.close();
}

int main(int argc, char **argv)
{
#ifndef WIN32
	setenv("GEO_NO_SIGNAL_HANDLER", "1", 1);
#endif

	GEO::initialize();

    // Import standard command line arguments, and custom ones
	GEO::CmdLine::import_arg_group("standard");
	GEO::CmdLine::import_arg_group("pre");
	GEO::CmdLine::import_arg_group("algo");


	igl::opengl::glfw::Viewer viewer;

	CLI::App command_line{"bc_settter"};
	std::string path = "";
	command_line.add_option("--mesh,-m", path, "Path to the input mesh");

    try {
        command_line.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return command_line.exit(e);
    }

	int current_id = 1;
	bool tracking_mouse = false;

	bool is_volume = true;

	MatrixXd V;
	MatrixXi F;
	MatrixXd p0, p1;
	MatrixXd N;
	MatrixXi adj;
	VectorXi selected;
	MatrixXd C;
	Matrix<bool, Eigen::Dynamic, 1> visited;
	Matrix<std::vector<int>, Dynamic, 1> all_2_local;
	VectorXi boundary_2_all;

	BCVals vals;
	vals.reset();


	if(!path.empty())
	{
		is_volume = load(path, viewer, V, F, p0, p1, N, adj, selected, all_2_local, boundary_2_all, C);
		visited.resize(F.rows());
	}

	igl::opengl::glfw::imgui::ImGuiMenu menu;
	viewer.plugins.push_back(&menu);

	menu.callback_draw_viewer_window = []() { };
	menu.callback_draw_viewer_menu = []() { };
	menu.callback_draw_custom_window = [&]()
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Open", "Ctrl+O"))
				{
					std::string fname = igl::file_dialog_open();

					if (fname.length() == 0)
						return;

					is_volume = load(fname, viewer, V, F, p0, p1, N, adj, selected, all_2_local, boundary_2_all, C);
					visited.resize(F.rows());

					vals.reset();


					viewer.data().clear();
					viewer.data().add_edges(p0, p1, RowVector3d(0,0,0));
					viewer.data().set_mesh(V, F);
					viewer.data().set_colors(C);
					viewer.core.align_camera_center(V);
				}
				if (ImGui::MenuItem("Save", "Ctrl+S"))
				{
					std::string fname = igl::file_dialog_save();

					if(fname.length() == 0)
						return;

					save(fname, selected, vals);
				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}

		const float ui_scaling_factor = menu.hidpi_scaling() / menu.pixel_ratio();
		const float menu_width = 180 * ui_scaling_factor;

		ImGui::SetNextWindowPos(ImVec2(5,20), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiSetCond_FirstUseEver);
  		ImGui::SetNextWindowSizeConstraints(ImVec2(menu_width, -1.0f), ImVec2(menu_width, -1.0f));

		static bool show_file_menu = true;
		ImGui::Begin("Boundary conditions", &show_file_menu, WINDOW_FLAGS );

		std::vector<std::string> items;
		items.push_back("Clear");
		for(int i = 1; i <= int(vals.size()); ++i){
			std::string title = "ID " + std::to_string(i);
			items.push_back(title);
		}
        static int item_current = 0;
		ImGui::Combo("Boundary", &current_id, items);

		ImGui::Separator();
		for(int i = 1; i <= int(vals.size()); ++i){
			std::string label = std::to_string(i);

			std::string title = "Boundary " + label;
			// std::string rlabel = "##bc_selector";

			std::string vlabel = "Value##idvalue" + label;
			std::string flabel = "Function##idvalue" + label;

			std::string dlabel = "Dirichlet##idtype" + label;
			std::string nlabel = "Neuman##idtype" + label;

			std::string xlabel = "x##identry" + label;
			std::string ylabel = "y##identry" + label;
			std::string zlabel = "z##identry" + label;

			if (ImGui::TreeNode(title.c_str()))
			{

				ImGui::TextColored(ImVec4(color(i, vals.size())(0),color(i, vals.size())(1),color(i, vals.size())(2),1.0f), "%s", "Color");


			// ImGui::RadioButton(rlabel.c_str(), &current_id, i);

				ImGui::RadioButton(vlabel.c_str(), &vals.bc_value[i-1], 0); ImGui::SameLine();
				ImGui::RadioButton(flabel.c_str(), &vals.bc_value[i-1], 1);

				ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.20f);

				if(vals.bc_value[i-1] == 0)
				{
					ImGui::InputFloat(xlabel.c_str(), &vals.vals[i-1][0], 0, 0, 3); ImGui::SameLine();
					ImGui::InputFloat(ylabel.c_str(), &vals.vals[i-1][1], 0, 0, 3);
					if(is_volume){
						ImGui::SameLine();
						ImGui::InputFloat(zlabel.c_str(), &vals.vals[i-1][2], 0, 0, 3);
					}
				}
				else
				{
					ImGui::InputText(xlabel.c_str(), vals.funs[i-1](0).data(), BUF_SIZE); ImGui::SameLine();
					ImGui::InputText(ylabel.c_str(), vals.funs[i-1](1).data(), BUF_SIZE);
					if(is_volume)
					{
						ImGui::SameLine();
						ImGui::InputText(zlabel.c_str(), vals.funs[i-1](2).data(), BUF_SIZE);
					}
				}
				ImGui::PopItemWidth();

				ImGui::RadioButton(dlabel.c_str(), &vals.bc_type[i-1], 0); ImGui::SameLine();
				ImGui::RadioButton(nlabel.c_str(), &vals.bc_type[i-1], 1);

				ImGui::TreePop();
			}
		}
		ImGui::Separator();

		if(ImGui::Button("Add ID"))
		{
			vals.append();

			for(int bindex = 0; bindex < selected.size(); ++bindex)
			{
				const int v = selected(bindex);

				for(int i : all_2_local(bindex))
					C.row(i) = color(v, vals.size());
			}

			current_id = vals.size();
			viewer.data().set_colors(C);
		}

		ImGui::End();
	};

	auto paint = [&](bool flod_fill) {
		int fid;
		Vector3f bc;

		// Cast a ray in the view direction starting from the mouse position
		double x = viewer.current_mouse_x;
		double y = viewer.core.viewport(3) - viewer.current_mouse_y;
		if(igl::unproject_onto_mesh(Vector2f(x,y), viewer.core.view,
			viewer.core.proj, viewer.core.viewport, V, F, fid, bc))
		{
			if(is_volume)
			{
				if(!flod_fill)
				{
					const int real_face = boundary_2_all(fid);
					selected(real_face) = current_id;
					const auto &loc_faces = all_2_local(real_face);

					const auto col = color(selected[real_face], vals.size());

					for(int i : loc_faces){
						C.row(i) = col;
					}
				}
				else
				{
					visited.setConstant(false);
					std::queue<int> to_visit; to_visit.push(fid);

					while(!to_visit.empty())
					{
						const int id = to_visit.front();
						to_visit.pop();

						if(visited(id))
							continue;

						visited(id) = true;

						const int real_face = boundary_2_all(id);
						selected(real_face) = current_id;
						const auto &loc_faces = all_2_local(real_face);

						const auto col = color(selected[real_face], vals.size());

						for(int i : loc_faces){
							C.row(i) = col;
						}

						assert(id<adj.size());
					// auto &neighs = adj[id];
						for(int i = 0; i < 3; ++i)
						{
							const int nid = adj(id, i);
							if(visited(nid))
								continue;

							if(std::abs(N.row(fid).dot(N.row(nid)))<0.99)
								continue;

							to_visit.push(nid);
						}
					}
				}

				viewer.data().set_colors(C);
			}
			else
			{
				//TODO

				const auto min_bc = bc.minCoeff();

				if(min_bc < 0.1)
				{
					// const int index = min_bc == bc[0] ? 0 : (min_bc == bc[1] ? 1 : 2);
					// const int id0 = F(fid, (index + 1)%3);
					// const int id1 = F(fid, (index + 2)%3);

					const Eigen::MatrixXd p = V.row(F(fid, 0))*bc(0) + V.row(F(fid, 1))*bc(1) + V.row(F(fid, 2))*bc(2);
					double min_dist = std::numeric_limits<double>::max();

					int eid = -1;

					for(size_t i = 0; i < p1.rows(); ++i)
					{
						double d = (p - p1.row(i)).norm();

						if(d < min_dist)
						{
							eid = i;
							min_dist = d;
						}

						d = (p - p0.row(i)).norm();

						if(d < min_dist)
						{
							eid = i;
							min_dist = d;
						}
					}

					assert(eid >= 0);

					const int real_edge = boundary_2_all(eid);

					// if(!all_2_local[real_edge].empty())
					{
						selected(real_edge) = current_id;
						const auto col = color(selected[real_edge], vals.size());
						C.row(eid) = col;
					}

					viewer.data().set_edges(p0, Eigen::MatrixXi(0, 2), RowVector3d(0,0,0));
					viewer.data().add_edges(p0, p1, C);
				}
			}

			tracking_mouse = true;
			return true;
		}
		return false;
	};

	int current_modifier = -1;

	viewer.callback_mouse_down = [&](igl::opengl::glfw::Viewer& viewer, int, int modifier)->bool
	{
		//shift or command
		if(modifier != 1 && modifier !=8 )
			return false;

		current_modifier = modifier;
		return paint(current_modifier == 1);
	};

	viewer.callback_mouse_move = [&](igl::opengl::glfw::Viewer& viewer, int, int)->bool
	{
		if(!tracking_mouse)
			return false;

		return paint(current_modifier == 1);
	};

	viewer.callback_mouse_up = [&](igl::opengl::glfw::Viewer& viewer, int, int)->bool
	{
		tracking_mouse = false;
		return false;
	};

	if(V.size() > 0)
	{
		viewer.data().add_edges(p0, p1, RowVector3d(0,0,0));
		viewer.data().set_mesh(V, F);
		if(is_volume)
			viewer.data().set_colors(C);
		viewer.core.align_camera_center(V);
	}
	viewer.data().show_lines = false;
	viewer.launch();
}