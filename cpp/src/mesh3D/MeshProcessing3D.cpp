#include "MeshProcessing3D.hpp"
#include <algorithm>
#include <map>
#include<set>
#include<queue>
#include <iterator>
#include <cassert>
#include "Eigen/Dense"

using namespace poly_fem::MeshProcessing3D;
using namespace poly_fem;
using namespace std;
using namespace Eigen;

void MeshProcessing3D::build_connectivity(Mesh3DStorage &hmi) {
	hmi.edges.clear();
	if (hmi.type == MeshType::Tri || hmi.type == MeshType::Qua || hmi.type == MeshType::HSur) {
		std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> temp;
		temp.reserve(hmi.faces.size() * 3);
		for (uint32_t i = 0; i < hmi.faces.size(); ++i) {
			int vn = hmi.faces[i].vs.size();
			for (uint32_t j = 0; j < vn; ++j) {
				uint32_t v0 = hmi.faces[i].vs[j], v1 = hmi.faces[i].vs[(j + 1) % vn];
				if (v0 > v1) std::swap(v0, v1);
				temp.push_back(std::make_tuple(v0, v1, i, j));
			}
			hmi.faces[i].es.resize(vn);
		}
		std::sort(temp.begin(), temp.end());
		hmi.edges.reserve(temp.size() / 2);
		uint32_t E_num = 0;
		Edge e; e.boundary = true; e.vs.resize(2);
		for (uint32_t i = 0; i < temp.size(); ++i) {
			if (i == 0 || (i != 0 && (std::get<0>(temp[i]) != std::get<0>(temp[i - 1]) ||
				std::get<1>(temp[i]) != std::get<1>(temp[i - 1])))) {
				e.id = E_num; E_num++;
				e.vs[0] = std::get<0>(temp[i]);
				e.vs[1] = std::get<1>(temp[i]);
				hmi.edges.push_back(e);
			}
			else if (i != 0 && (std::get<0>(temp[i]) == std::get<0>(temp[i - 1]) &&
				std::get<1>(temp[i]) == std::get<1>(temp[i - 1])))
				hmi.edges[E_num - 1].boundary = false;

			hmi.faces[std::get<2>(temp[i])].es[std::get<3>(temp[i])] = E_num - 1;
		}
		//boundary
		for (auto &v : hmi.vertices) v.boundary = false;
		for (uint32_t i = 0; i < hmi.edges.size(); ++i)
			if (hmi.edges[i].boundary) {
				hmi.vertices[hmi.edges[i].vs[0]].boundary = hmi.vertices[hmi.edges[i].vs[1]].boundary = true;
			}
	}else if (hmi.type == MeshType::Hyb) {
		vector<bool> bf_flag(hmi.faces.size(), false);
		for (auto h : hmi.elements) for (auto f : h.fs)bf_flag[f] = !bf_flag[f];
		for (auto &f : hmi.faces) f.boundary = bf_flag[f.id];

		std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> temp;
		for (uint32_t i = 0; i < hmi.faces.size(); ++i) {
			int fl = hmi.faces[i].vs.size();
			for (uint32_t j = 0; j < hmi.faces[i].vs.size(); ++j) {
				uint32_t v0 = hmi.faces[i].vs[j], v1 = hmi.faces[i].vs[(j + 1) % fl];
				if (v0 > v1) std::swap(v0, v1);
				temp.push_back(std::make_tuple(v0, v1, i, j));
			}
			hmi.faces[i].es.resize(fl);
		}
		std::sort(temp.begin(), temp.end());
		hmi.edges.reserve(temp.size() / 2);
		uint32_t E_num = 0;
		Edge e; e.boundary = false; e.vs.resize(2);
		for (uint32_t i = 0; i < temp.size(); ++i) {
			if (i == 0 || (i != 0 && (std::get<0>(temp[i]) != std::get<0>(temp[i - 1]) ||
				std::get<1>(temp[i]) != std::get<1>(temp[i - 1])))) {
				e.id = E_num; E_num++;
				e.vs[0] = std::get<0>(temp[i]);
				e.vs[1] = std::get<1>(temp[i]);
				hmi.edges.push_back(e);
			}
			hmi.faces[std::get<2>(temp[i])].es[std::get<3>(temp[i])] = E_num - 1;
		}
		//boundary
		for (auto &v : hmi.vertices) v.boundary = false;
		for (uint32_t i = 0; i < hmi.faces.size(); ++i)
			if (hmi.faces[i].boundary) for (uint32_t j = 0; j < hmi.faces[i].vs.size(); ++j) {
				uint32_t eid = hmi.faces[i].es[j];
				hmi.edges[eid].boundary = true;
				hmi.vertices[hmi.faces[i].vs[j]].boundary = true;
			}
	}
	//f_nhs;
	for (auto &f : hmi.faces)f.neighbor_hs.clear();
	for (uint32_t i = 0; i < hmi.elements.size(); i++) {
		for (uint32_t j = 0; j < hmi.elements[i].fs.size(); j++) {
			hmi.faces[hmi.elements[i].fs[j]].neighbor_hs.push_back(i);
		}
	}
	//e_nfs, v_nfs
	for (auto &e : hmi.edges) e.neighbor_fs.clear();
	for (auto &v : hmi.vertices) v.neighbor_fs.clear();
	for (uint32_t i = 0; i < hmi.faces.size(); i++) {
		for (uint32_t j = 0; j < hmi.faces[i].es.size(); j++) hmi.edges[hmi.faces[i].es[j]].neighbor_fs.push_back(i);
		for (uint32_t j = 0; j < hmi.faces[i].vs.size(); j++) hmi.vertices[hmi.faces[i].vs[j]].neighbor_fs.push_back(i);
	}
	//v_nes, v_nvs
	for (auto &v : hmi.vertices) {
		v.neighbor_es.clear();
		v.neighbor_vs.clear();
	}
	for (uint32_t i = 0; i < hmi.edges.size(); i++) {
		uint32_t v0 = hmi.edges[i].vs[0], v1 = hmi.edges[i].vs[1];
		hmi.vertices[v0].neighbor_es.push_back(i);
		hmi.vertices[v1].neighbor_es.push_back(i);
		hmi.vertices[v0].neighbor_vs.push_back(v1);
		hmi.vertices[v1].neighbor_vs.push_back(v0);
	}
	//e_nhs
	for (auto &e : hmi.edges) e.neighbor_hs.clear();
	for (uint32_t i = 0; i < hmi.edges.size(); i++) {
		std::vector<uint32_t> nhs;
		for (uint32_t j = 0; j < hmi.edges[i].neighbor_fs.size(); j++) {
			uint32_t nfid = hmi.edges[i].neighbor_fs[j];
			nhs.insert(nhs.end(), hmi.faces[nfid].neighbor_hs.begin(), hmi.faces[nfid].neighbor_hs.end());
		}
		std::sort(nhs.begin(), nhs.end()); nhs.erase(std::unique(nhs.begin(), nhs.end()), nhs.end());
		hmi.edges[i].neighbor_hs = nhs;
	}
	//v_nhs; ordering fs for hex
	for (auto &v : hmi.vertices) v.neighbor_hs.clear();
	for (uint32_t i = 0; i < hmi.elements.size(); i++) {
		vector<uint32_t> vs;
		for (auto fid : hmi.elements[i].fs)vs.insert(vs.end(), hmi.faces[fid].vs.begin(), hmi.faces[fid].vs.end());
		sort(vs.begin(), vs.end()); vs.erase(unique(vs.begin(), vs.end()), vs.end());

		bool degree3 = true;
		for (auto vid : vs) {
			int nv = 0;
			for (auto nvid : hmi.vertices[vid].neighbor_vs) if (find(vs.begin(), vs.end(), nvid) != vs.end()) nv++;
			if (nv != 3) { degree3 = false; break; }
		}

		if (hmi.elements[i].hex && (vs.size() != 8 || !degree3))hmi.elements[i].hex = false;
		hmi.elements[i].vs.clear();

		hmi.elements[i].vs.clear();

		if (hmi.elements[i].hex) {
			int top_fid = hmi.elements[i].fs[0];
			hmi.elements[i].vs = hmi.faces[top_fid].vs;

			std::set<uint32_t> s_model(vs.begin(), vs.end());
			std::set<uint32_t> s_pattern(hmi.faces[top_fid].vs.begin(), hmi.faces[top_fid].vs.end());
			vector<uint32_t> vs_left;
			std::set_difference(s_model.begin(), s_model.end(), s_pattern.begin(), s_pattern.end(), std::back_inserter(vs_left));

			for (auto vid : hmi.faces[top_fid].vs)for (auto nvid : hmi.vertices[vid].neighbor_vs)
				if (find(vs_left.begin(), vs_left.end(), nvid) != vs_left.end()) {
					hmi.elements[i].vs.push_back(nvid); break;
				}

			function<int(vector<uint32_t> &, int &)> WHICH_F = [&](vector<uint32_t> &vs0, int &f_flag)->int {
				int which_f = -1;
				sort(vs0.begin(), vs0.end());
				bool found_f = false;
				for (uint32_t j = 0; j < hmi.elements[i].fs.size();j++) {
					auto fid = hmi.elements[i].fs[j];
					vector<uint32_t> vs1 = hmi.faces[fid].vs;
					sort(vs1.begin(), vs1.end());
					if (vs0.size() == vs1.size() && std::equal(vs0.begin(), vs0.end(), vs1.begin())) {
						f_flag = hmi.elements[i].fs_flag[j];
						which_f = fid; break;
					}
				}
				return which_f;
			};

			vector<uint32_t> fs;
			vector<bool> fs_flag;
			fs_flag.push_back(hmi.elements[i].fs_flag[0]);
			fs.push_back(top_fid);
			vector<uint32_t> vs_temp;

			vs_temp.insert(vs_temp.end(), hmi.elements[i].vs.begin() + 4, hmi.elements[i].vs.end());
			int f_flag = -1;
			int bottom_fid = WHICH_F(vs_temp, f_flag);
			fs_flag.push_back(f_flag);
			fs.push_back(bottom_fid);

			vs_temp.clear();
			vs_temp.push_back(hmi.elements[i].vs[0]);
			vs_temp.push_back(hmi.elements[i].vs[1]);
			vs_temp.push_back(hmi.elements[i].vs[4]);
			vs_temp.push_back(hmi.elements[i].vs[5]);
			f_flag = -1;
			int front_fid = WHICH_F(vs_temp, f_flag);
			fs_flag.push_back(f_flag);
			fs.push_back(front_fid);

			vs_temp.clear();
			vs_temp.push_back(hmi.elements[i].vs[2]);
			vs_temp.push_back(hmi.elements[i].vs[3]);
			vs_temp.push_back(hmi.elements[i].vs[6]);
			vs_temp.push_back(hmi.elements[i].vs[7]);
			f_flag = -1;
			int back_fid = WHICH_F(vs_temp, f_flag);
			fs_flag.push_back(f_flag);
			fs.push_back(back_fid);

			vs_temp.clear();
			vs_temp.push_back(hmi.elements[i].vs[1]);
			vs_temp.push_back(hmi.elements[i].vs[2]);
			vs_temp.push_back(hmi.elements[i].vs[5]);
			vs_temp.push_back(hmi.elements[i].vs[6]);
			f_flag = -1;
			int left_fid = WHICH_F(vs_temp, f_flag);
			fs_flag.push_back(f_flag);
			fs.push_back(left_fid);

			vs_temp.clear();
			vs_temp.push_back(hmi.elements[i].vs[3]);
			vs_temp.push_back(hmi.elements[i].vs[0]);
			vs_temp.push_back(hmi.elements[i].vs[7]);
			vs_temp.push_back(hmi.elements[i].vs[4]);
			f_flag = -1;
			int right_fid = WHICH_F(vs_temp, f_flag);
			fs_flag.push_back(f_flag);
			fs.push_back(right_fid);

			hmi.elements[i].fs = fs;
			hmi.elements[i].fs_flag = fs_flag;
		}
		else hmi.elements[i].vs = vs;

		for (uint32_t j = 0; j < hmi.elements[i].vs.size(); j++) hmi.vertices[hmi.elements[i].vs[j]].neighbor_hs.push_back(i);
	}
}

void MeshProcessing3D::refine_catmul_clark_polar(Mesh3DStorage &M, int iter) {
	for (int i = 0; i < iter; i++) {
		Mesh3DStorage M_;
		M_.type = MeshType::Hyb;

		vector<int> E2V(M.edges.size()), F2V(M.faces.size()), Ele2V(M.elements.size());

		int vn = 0;
		for (auto v : M.vertices) {
			Vertex v_;
			v_.id = vn++;
			v_.v.resize(3);
			for (int j = 0; j < 3; j++)v_.v[j] = M.points(j, v.id);
			M_.vertices.push_back(v_);
		}
		
		for (auto e : M.edges) {
			Vertex v;
			v.id = vn++;
			v.v.resize(3);

			Vector3d center;
			center.setZero();
			for (auto vid: e.vs) center += M.points.col(vid);
			center /= e.vs.size();
			for (int j = 0; j < 3; j++)v.v[j] = center[j];

			M_.vertices.push_back(v);
			E2V[e.id] = v.id;
		}
		for (auto f : M.faces) {
			Vertex v;
			v.id = vn++;
			v.v.resize(3);

			Vector3d center;
			center.setZero();
			for (auto vid : f.vs) center += M.points.col(vid);
			center /= f.vs.size();
			for (int j = 0; j < 3; j++)v.v[j] = center[j];

			M_.vertices.push_back(v);
			F2V[f.id] = v.id;
		}
		for (auto ele : M.elements) {
			if (!ele.hex) continue;
			Vertex v;
			v.id = vn++;
			v.v = ele.v_in_Kernel;

			M_.vertices.push_back(v);
			Ele2V[ele.id] = v.id;
		}
		//new elements
		std::vector<std::vector<uint32_t>> total_fs; total_fs.reserve(M.elements.size() * 8 * 6);
		std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>> tempF;
		tempF.reserve(M.elements.size() * 8 * 6);
		std::vector<uint32_t> vs(4);

		int elen = 0, fn = 0;
		for (auto ele : M.elements) {
			if (ele.hex) {
				for (auto vid : ele.vs) {
					//top 4 vs
					vector<int> top_vs(4);
					top_vs[0] = vid;
					int fid = -1;
					for (auto nfid : M.vertices[vid].neighbor_fs)if (find(ele.fs.begin(), ele.fs.end(), nfid) != ele.fs.end()) {
						fid = nfid; break;
					}
					assert(fid != -1);
					top_vs[2] = F2V[fid];
					
					int v_ind = find(M.faces[fid].vs.begin(), M.faces[fid].vs.end(),vid) - M.faces[fid].vs.begin();
					int e_pre = M.faces[fid].es[(v_ind - 1 + 4) % 4];
					int e_aft = M.faces[fid].es[v_ind];
					top_vs[1] = E2V[e_pre];
					top_vs[3] = E2V[e_aft];
					//bottom 4 vs
					vector<int> bottom_vs(4);

					auto nvs = M.vertices[vid].neighbor_vs;
					int e_per = -1;
					for (auto nvid : nvs) if (find(ele.vs.begin(), ele.vs.end(), nvid) != ele.vs.end()) {
						if (nvid != M.edges[e_pre].vs[0] && nvid != M.edges[e_pre].vs[1] &&
							nvid != M.edges[e_aft].vs[0] && nvid != M.edges[e_aft].vs[1]) {
							vector<uint32_t> sharedes, es0 = M.vertices[vid].neighbor_es, es1 = M.vertices[nvid].neighbor_es;
							sort(es0.begin(), es0.end()); sort(es1.begin(), es1.end());
							set_intersection(es0.begin(), es0.end(), es1.begin(), es1.end(), back_inserter(sharedes));
							assert(sharedes.size());
							e_per = sharedes[0];
							break;
						}
					}

					assert(e_per != -1);
					bottom_vs[0] = E2V[e_per];
					bottom_vs[2] = Ele2V[ele.id];

					int f_pre = -1;
					vector<uint32_t> sharedfs, fs0 = M.edges[e_pre].neighbor_fs, fs1 = M.edges[e_per].neighbor_fs;
					sort(fs0.begin(), fs0.end()); sort(fs1.begin(), fs1.end());
					set_intersection(fs0.begin(), fs0.end(), fs1.begin(), fs1.end(), back_inserter(sharedfs));
					for (auto sfid : sharedfs)if (find(ele.fs.begin(), ele.fs.end(), sfid) != ele.fs.end()) {
						f_pre = sfid; break;
					}
					assert(f_pre != -1);

					int f_aft = -1;
					sharedfs.clear();
					fs0 = M.edges[e_aft].neighbor_fs;
					fs1 = M.edges[e_per].neighbor_fs;
					sort(fs0.begin(), fs0.end()); sort(fs1.begin(), fs1.end());
					set_intersection(fs0.begin(), fs0.end(), fs1.begin(), fs1.end(), back_inserter(sharedfs));
					for (auto sfid : sharedfs)if (find(ele.fs.begin(), ele.fs.end(), sfid) != ele.fs.end()) {
						f_aft = sfid; break;
					}
					assert(f_aft != -1);

					bottom_vs[1] = F2V[f_pre];
					bottom_vs[3] = F2V[f_aft];

					vector<int> ele_vs = top_vs;
					ele_vs.insert(ele_vs.end(), bottom_vs.begin(), bottom_vs.end());
					//fs
					for (short j = 0; j < 6; j++) {
						for (short k = 0; k < 4; k++) vs[k] = ele_vs[hex_face_table[j][k]];
						total_fs.push_back(vs);
						std::sort(vs.begin(), vs.end());
						tempF.push_back(std::make_tuple(vs[0], vs[1], vs[2], vs[3], fn++, elen, j));
					}
					//new ele
					Element ele_;
					ele_.id = elen++;
					ele_.fs.resize(6, -1);
					ele_.fs_flag.resize(6, 1);

					ele_.hex = true;
					ele_.v_in_Kernel.resize(3);

					Vector3d center;
					center.setZero();
					for (auto evid : ele_vs) for (int j = 0; j < 3; j++)center[j] += M_.vertices[evid].v[j];
					center /= ele_vs.size();
					for (int j = 0; j < 3; j++)ele_.v_in_Kernel[j] = center[j];

					M_.elements.push_back(ele_);
				}
			}
			else {
				//local_V2V
				std::map<int, int> local_V2V;
				for (auto vid : ele.vs) {
					Vertex v_;
					v_.id = vn++;
					v_.v.resize(3);

					for (int j = 0; j < 3; j++)v_.v[j] = (M_.vertices[vid].v[j] + ele.v_in_Kernel[j])*0.5;
					M_.vertices.push_back(v_);

					local_V2V[vid] = v_.id;
 				}
				//local_E2V
				vector<uint32_t> es;
				for (auto fid : ele.fs)es.insert(es.end(), M.faces[fid].es.begin(), M.faces[fid].es.end());
				sort(es.begin(), es.end());
				es.erase(unique(es.begin(), es.end()), es.end());

				std::map<int, int> local_E2V;
				for (auto eid : es) {
					Vertex v_;
					v_.id = vn++;
					v_.v.resize(3);

					Vector3d center;
					center.setZero();
					for (auto vid : M.edges[eid].vs) for (int j = 0; j < 3;j++) center[j] += M_.vertices[local_V2V[vid]].v[j];
					center /= M.edges[eid].vs.size();
					for (int j = 0; j < 3; j++)v_.v[j] = center[j];

					M_.vertices.push_back(v_);

					local_E2V[eid] = v_.id;
				}
				//local_F2V
				std::map<int, int> local_F2V;
				for (auto fid : ele.fs) {
					Vertex v_;
					v_.id = vn++;
					v_.v.resize(3);

					Vector3d center;
					center.setZero();
					for (auto vid : M.faces[fid].vs) for (int j = 0; j < 3; j++) center[j] += M_.vertices[local_V2V[vid]].v[j];
					center /= M.faces[fid].vs.size();
					for (int j = 0; j < 3; j++)v_.v[j] = center[j];

					M_.vertices.push_back(v_);

					local_F2V[fid] = v_.id;
				}
				//polyhedron fs
				int local_fn = 0;
				for (auto fid:ele.fs) {
					auto &fvs = M.faces[fid].vs;
					auto &fes = M.faces[fid].es;
					int fvn = M.faces[fid].vs.size();
					for (uint32_t j = 0; j < fvn; j++) {
						vs[0] = local_E2V[fes[(j - 1 + fvn) % fvn]];
						vs[1] = local_V2V[fvs[j]];
						vs[2] = local_E2V[fes[j]];
						vs[3] = local_F2V[fid];

						total_fs.push_back(vs);
						std::sort(vs.begin(), vs.end());
						tempF.push_back(std::make_tuple(vs[0], vs[1], vs[2], vs[3], fn++, elen, local_fn++));
					}
				}
				//polyhedron
				Element ele_;
				ele_.id = elen++;
				ele_.fs.resize(local_fn, -1);
				ele_.fs_flag.resize(local_fn, 1);

				ele_.hex = false;
				ele_.v_in_Kernel = ele.v_in_Kernel;
				M_.elements.push_back(ele_);

				//hex
				for (auto fid : ele.fs) {
					auto &fvs = M.faces[fid].vs;
					auto &fes = M.faces[fid].es;
					int fvn = M.faces[fid].vs.size();
					for (uint32_t j = 0; j < fvn; j++) {
						vector<int> ele_vs(8);
						ele_vs[0] = local_E2V[fes[(j - 1 + fvn) % fvn]];
						ele_vs[1] = local_V2V[fvs[j]];
						ele_vs[2] = local_E2V[fes[j]];
						ele_vs[3] = local_F2V[fid];

						ele_vs[4] = E2V[fes[(j - 1 + fvn) % fvn]];
						ele_vs[5] = fvs[j];
						ele_vs[6] = E2V[fes[j]];
						ele_vs[7] = F2V[fid];

						//fs
						for (short j = 0; j < 6; j++) {
							for (short k = 0; k < 4; k++) vs[k] = ele_vs[hex_face_table[j][k]];
							total_fs.push_back(vs);
							std::sort(vs.begin(), vs.end());
							tempF.push_back(std::make_tuple(vs[0], vs[1], vs[2], vs[3], fn++, elen, j));
						}
						//hex
						Element ele_;
						ele_.id = elen++;
						ele_.fs.resize(6, -1);
						ele_.fs_flag.resize(6, 1);
						ele_.hex = true;
						ele_.v_in_Kernel.resize(3);

						Vector3d center;
						center.setZero();
						for (auto vid : ele_vs) for (int j = 0; j < 3; j++) center[j] += M_.vertices[vid].v[j];
						center /= ele_vs.size();
						for (int j = 0; j < 3; j++)ele_.v_in_Kernel[j] = center[j];
						M_.elements.push_back(ele_);
					}
				}
			}
		}
		//Fs
		std::sort(tempF.begin(), tempF.end());
		M_.faces.reserve(tempF.size() / 3);
		Face f; f.boundary = true;
		uint32_t F_num = 0;
		for (uint32_t i = 0; i < tempF.size(); ++i) {
			if (i == 0 || (i != 0 &&
				(std::get<0>(tempF[i]) != std::get<0>(tempF[i - 1]) || std::get<1>(tempF[i]) != std::get<1>(tempF[i - 1]) ||
					std::get<2>(tempF[i]) != std::get<2>(tempF[i - 1]) || std::get<3>(tempF[i]) != std::get<3>(tempF[i - 1])))) {
				f.id = F_num; F_num++;
				f.vs = total_fs[std::get<4>(tempF[i])];
				M_.faces.push_back(f);
			}
			else if (i != 0 && (std::get<0>(tempF[i]) == std::get<0>(tempF[i - 1]) && std::get<1>(tempF[i]) == std::get<1>(tempF[i - 1]) &&
				std::get<2>(tempF[i]) == std::get<2>(tempF[i - 1]) && std::get<3>(tempF[i]) == std::get<3>(tempF[i - 1])))
				M_.faces[F_num - 1].boundary = false;

			M_.elements[std::get<5>(tempF[i])].fs[std::get<6>(tempF[i])] = F_num - 1;
		}

		M_.points.resize(3, M_.vertices.size());
		for (auto v : M_.vertices)for (int j = 0; j < 3; j++)M_.points(j, v.id) = v.v[j];

		build_connectivity(M_);
		//surface orienting
		Mesh3DStorage M_sur; M_sur.type = MeshType::HSur;
		int bvn = 0;
		for (auto v : M_.vertices)if (v.boundary)bvn++;
		M_sur.points.resize(3, bvn);
		bvn = 0;
		vector<int> V_map(M_.vertices.size(),-1), V_map_reverse;
		for (auto v : M_.vertices)if (v.boundary) {
			M_sur.points.col(bvn++) = M_.points.col(v.id);
			Vertex v_;
			v_.id = M_sur.vertices.size();
			M_sur.vertices.push_back(v_);

			V_map[v.id] = v_.id; V_map_reverse.push_back(v.id);
		}
		for (auto f : M_.faces)if (f.boundary) {
			for (int j = 0; j < f.vs.size(); j++) {
				f.id = M_sur.faces.size();
				f.es.clear();
				f.neighbor_hs.clear();
				f.vs[j] = V_map[f.vs[j]];
				M_sur.vertices[f.vs[j]].neighbor_fs.push_back(f.id);
			}
			M_sur.faces.push_back(f);
		}
		build_connectivity(M_sur);
		orient_surface_mesh(M_sur);
		//volume orienting
		vector<bool> F_tag(M_.faces.size(), true);
		std::vector<short> F_visit(M_.faces.size(), 0);//0 un-visited, 1 visited once, 2 visited twice
		for (uint32_t j = 0; j < M_.faces.size(); j++)if (M_.faces[j].boundary) { F_visit[j]++; }
		std::vector<bool> F_state(M_.faces.size(), false);//false is the reverse direction, true is the same direction
		std::vector<bool> P_visit(M_.elements.size(), false);
		while (true) {
			std::vector<uint32_t> candidates;
			for (uint32_t j = 0; j < F_visit.size(); j++)if (F_visit[j] == 1)candidates.push_back(j);
			if (!candidates.size()) break;
			for (auto ca : candidates) {
				if (F_visit[ca] == 2) continue;
				uint32_t pid = M_.faces[ca].neighbor_hs[0];
				if (P_visit[pid]) if (M_.faces[ca].neighbor_hs.size() == 2) pid = M_.faces[ca].neighbor_hs[1];
				if (P_visit[pid]) {
					cout << "bug" << endl;
				}
				auto &fs = M_.elements[pid].fs;
				for (auto fid : fs) F_tag[fid] = false;

				uint32_t start_f = ca;
				F_tag[start_f] = true; F_visit[ca]++; if (F_state[ca]) F_state[ca] = false; else F_state[ca] = true;

				std::queue<uint32_t> pf_temp; pf_temp.push(start_f);
				while (!pf_temp.empty()) {
					uint32_t fid = pf_temp.front(); pf_temp.pop();
					for (auto eid : M_.faces[fid].es) for (auto nfid : M_.edges[eid].neighbor_fs) {

						if (F_tag[nfid]) continue;
						uint32_t v0 = M_.edges[eid].vs[0], v1 = M_.edges[eid].vs[1];
						int32_t v0_pos = std::find(M_.faces[fid].vs.begin(), M_.faces[fid].vs.end(), v0) - M_.faces[fid].vs.begin();
						int32_t v1_pos = std::find(M_.faces[fid].vs.begin(), M_.faces[fid].vs.end(), v1) - M_.faces[fid].vs.begin();

						if ((v0_pos + 1) % M_.faces[fid].vs.size() != v1_pos) std::swap(v0, v1);

						int32_t v0_pos_ = std::find(M_.faces[nfid].vs.begin(), M_.faces[nfid].vs.end(), v0) - M_.faces[nfid].vs.begin();
						int32_t v1_pos_ = std::find(M_.faces[nfid].vs.begin(), M_.faces[nfid].vs.end(), v1) - M_.faces[nfid].vs.begin();

						if (F_state[fid]) {
							if ((v0_pos_ + 1) % M_.faces[nfid].vs.size() == v1_pos_) F_state[nfid] = false;
							else F_state[nfid] = true;
						}
						else if (!F_state[fid]) {
							if ((v0_pos_ + 1) % M_.faces[nfid].vs.size() == v1_pos_) F_state[nfid] = true;
							else F_state[nfid] = false;
						}

						F_visit[nfid]++;

						pf_temp.push(nfid); F_tag[nfid] = true;
					}
				}
				P_visit[pid] = true;
				for (uint32_t j = 0; j < fs.size();j++) M_.elements[pid].fs_flag[j] = F_state[fs[j]];
			}
		}

		build_connectivity(M_);
		M = M_;
	}

}
void  MeshProcessing3D::orient_surface_mesh(Mesh3DStorage &hmi) {

	vector<bool> flag(hmi.faces.size(), true);
	flag[0] = false;

	std::queue<uint32_t> pf_temp; pf_temp.push(0);
	while (!pf_temp.empty()) {
		uint32_t fid = pf_temp.front(); pf_temp.pop();
		for (auto eid : hmi.faces[fid].es) for (auto nfid : hmi.edges[eid].neighbor_fs) {
			if (!flag[nfid]) continue;
			uint32_t v0 = hmi.edges[eid].vs[0], v1 = hmi.edges[eid].vs[1];
			int32_t v0_pos = std::find(hmi.faces[fid].vs.begin(), hmi.faces[fid].vs.end(), v0) - hmi.faces[fid].vs.begin();
			int32_t v1_pos = std::find(hmi.faces[fid].vs.begin(), hmi.faces[fid].vs.end(), v1) - hmi.faces[fid].vs.begin();

			if ((v0_pos + 1) % hmi.faces[fid].vs.size() != v1_pos) swap(v0, v1);

			int32_t v0_pos_ = std::find(hmi.faces[nfid].vs.begin(), hmi.faces[nfid].vs.end(), v0) - hmi.faces[nfid].vs.begin();
			int32_t v1_pos_ = std::find(hmi.faces[nfid].vs.begin(), hmi.faces[nfid].vs.end(), v1) - hmi.faces[nfid].vs.begin();

			if ((v0_pos_ + 1) % hmi.faces[nfid].vs.size() == v1_pos_) std::reverse(hmi.faces[nfid].vs.begin(), hmi.faces[nfid].vs.end());

			pf_temp.push(nfid); flag[nfid] = false;
		}
	}
	double res = 0;
	Vector3d ori; ori.setZero();
	for (auto f : hmi.faces) {
		auto &fvs = f.vs;
		Vector3d center; center.setZero(); for (auto vid : fvs) center += hmi.points.col(vid); center /= fvs.size();

		for (uint32_t j = 0; j < fvs.size(); j++) {
			Vector3d x = hmi.points.col(fvs[j]) - ori, y = hmi.points.col(fvs[(j + 1) % fvs.size()]) - ori, z = center - ori;
			res += -((x[0] * y[1] * z[2] + x[1] * y[2] * z[0] + x[2] * y[0] * z[1]) - (x[2] * y[1] * z[0] + x[1] * y[0] * z[2] + x[0] * y[2] * z[1]));
		}
	}
	if (res > 0) {
		for (uint32_t i = 0; i < hmi.faces.size(); i++) std::reverse(hmi.faces[i].vs.begin(), hmi.faces[i].vs.end());
	}
}