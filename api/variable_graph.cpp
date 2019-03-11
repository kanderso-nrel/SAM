//
// Created by Guittet, Darice on 2019-03-04.
//
#include <iostream>
#include <fstream>

#include "variable_graph.h"
#include "data_structures.h"

vertex* digraph::add_vertex(std::string n, bool is_ssc){
    if (vertex* v = find_vertex(n, is_ssc))
        return v;
    vertex* v = new vertex(n, is_ssc);
    auto it = vertices.find(n);
    if (it == vertices.end()){
        std::vector<vertex*> vs(2, nullptr);
        vertices.insert({n, vs});
    }
    vertices.find(n)->second.at((size_t)is_ssc) = v;
    return v;
}

vertex* digraph::find_vertex(std::string n, bool is_ssc){
    auto it = vertices.find(n);
    if (it == vertices.end())
        return nullptr;
    std::vector<vertex*> vec = it->second;
    if (vertex* v = vec.at((size_t)is_ssc))
        return v;
    return nullptr;
}

vertex* digraph::find_vertex(vertex* v){
    return find_vertex(v->name, v->is_ssc_var);
}

void digraph::delete_vertex(vertex* v){
    assert(v);
    for (size_t i = 0; i < v->edges_in.size(); i++){
        delete_edge(v->edges_in[i]);
    }
    for (size_t i = 0; i < v->edges_out.size(); i++){
        delete_edge(v->edges_out[i]);
    }
    auto it = vertices.find(v->name)->second;
    if (it.at(0) == v){
        delete it.at(0);
    }
    else
        delete it.at(1);
}

edge* digraph::find_edge(std::string src_name, bool src_is_ssc, std::string dest_name, bool dest_is_ssc, int type){
    vertex* src = find_vertex(src_name, src_is_ssc);
    vertex* dest = find_vertex(dest_name, dest_is_ssc);
    if (!src || !dest)
        return nullptr;

    if (edge* e = src->get_edge_out_to(dest)){
        return e;
    }
    else
        return nullptr;
}

bool digraph::add_edge(vertex* src, vertex* dest, int type, std::string obj, std::string expression){
    if (!src || !dest){
        std::cout << "/* digraph::add_edge error: vertices null */ \n";
        return false;
    }
    edge* e = new edge(src, dest, type, obj, expression);
    src->edges_out.push_back(e);
    dest->edges_in.push_back(e);
    return true;
}


bool digraph::add_edge(std::string src, bool src_is_ssc, std::string dest, bool dest_is_ssc,
              int type, std::string obj, std::string expression) {
    assert(type >= 0);

    if (find_edge(src, src_is_ssc, dest, dest_is_ssc, type))
        return true;
    if (find_edge(dest, dest_is_ssc, src, src_is_ssc, type))
        return true;
    if (std::strcmp(dest.c_str(), src.c_str()) == 0 && src_is_ssc == dest_is_ssc){
        return false;
    }

    vertex* v1 = find_vertex(src, src_is_ssc);
    vertex* v2 = find_vertex(dest, dest_is_ssc);
    if (!v1 || !v2){
        std::cout << "/* digraph::add_edge error: could not find vertices ";
        std::cout << (!v1? src + " " + std::to_string(src_is_ssc)
                         : dest + " " + std::to_string(dest_is_ssc) ) << " */\n";
        return false;
    }

    return add_edge(v1, v2, type, obj, expression);
}


void digraph::delete_edge(edge* e){
    assert(e);
    vertex* src = e->src;
    for (size_t i = 0; i < src->edges_out.size(); i++){
        if (src->edges_out[i] == e){
            src->edges_out.erase(src->edges_out.begin() + i);
            break;
        }
    }
    vertex* dest = e->dest;
    for (size_t i = 0; i < dest->edges_in.size(); i++){
        if (dest->edges_in[i] == e){
            dest->edges_in.erase(dest->edges_in.begin() + i);
            break;
        }
    }
    delete e;
}

// rename vertices map key and vertex itself
void digraph::rename_vertex(std::string old, bool is_ssc, std::string n){
    auto old_it = vertices.find(old);
    if (old_it == vertices.end()){
        std::cout << "digraph::rename_vertex error: could not find \'" << old << "\' vertex\n";
        assert(false);
    }

    // new entry by new name
    vertices.insert({n, std::vector<vertex*>(2, nullptr)});
    std::vector<vertex*>& new_vec = vertices.find(n)->second;

    // move ownership of pointer & rename vertex and its edges
    new_vec.at((size_t)is_ssc) = old_it->second.at((size_t)is_ssc);
    vertex* v = new_vec.at((size_t)is_ssc);
    assert(v);
    v->name = n;
    for (size_t i = 0; i < v->edges_in.size(); i++){
        edge* e = v->edges_in[i];
        if (e->obj_name.find("tbd") != std::string::npos){
            size_t pos = e->obj_name.find("tbd");
            e->obj_name.replace(pos, 3, n);
        }
    }
    for (size_t i = 0; i < v->edges_out.size(); i++){
        edge* e = v->edges_out[i];
        if (e->obj_name.find("tbd") != std::string::npos){
            size_t pos = e->obj_name.find("tbd");
            e->obj_name.replace(pos, 3, n);
        }
    }
    old_it->second.at((size_t)is_ssc) = nullptr;

    // if no vertices by the old name, delete hash entry
    if (!old_it->second.at(0) && !old_it->second.at(1))
        vertices.erase(old_it);
}

// vertices inserted as tbd:var will be rename to cmod:var, with duplication check
void digraph::rename_cmod_vertices(std::string cmod_name){
    size_t not_ssc_var = 0; // secondary cmod variables are not primary
    for (auto it = vertices.begin(); it != vertices.end(); ++it){
        if (it->first.find("tbd:") != std::string::npos){

            std::string new_name = it->first;
            size_t pos = new_name.find("tbd:");
            new_name.replace(pos, 4, (cmod_name + ":"));

            rename_vertex(it->first, not_ssc_var, new_name);

        }
    }
}


bool digraph::copy_vertex_descendants(vertex *v){
    // if vertex has already been added, so has its descendants
    if (find_vertex(v))
        return true;
    bool add = false;

    if (v->edges_out.size() == 0 ){
        // if it's a terminal node and not an ssc var, don't copy
        if(!v->is_ssc_var)
            return false;
        // if it is a ssc var, add the vertex
        else{
            add_vertex(v->name, v->is_ssc_var);
            return true;
        }
    }

    // if vertex is ssc, add it now in case none of its descendants are ssc
    if (v->is_ssc_var){
        add_vertex(v->name, v->is_ssc_var);
        add = true;
    }

    for (size_t i = 0; i < v->edges_out.size(); i++){
        vertex* dest = v->edges_out[i]->dest;

        // if any of the descendants of this edge is ssc, copy
        if(copy_vertex_descendants(dest) ){
            add_vertex(v->name, v->is_ssc_var);
            edge* e = v->edges_out[i];
            add_edge(v->name, v->is_ssc_var, dest->name, dest->is_ssc_var, e->type, e->obj_name, e->expression);
            add = true;
        }
    }

    return add;
}

void digraph::subgraph_ssc_only(digraph& new_graph){
    for (auto it = vertices.begin(); it != vertices.end(); ++it){
        if (vertex* src = it->second.at(1)){
            if (src->edges_out.size() > 0){
                new_graph.copy_vertex_descendants(src);
            }
        }
        if (vertex* src = it->second.at(0)){
            if (SAM_cmod_to_outputs.find(src->name) != SAM_cmod_to_outputs.end()){

                new_graph.copy_vertex_descendants(src);
            }
        }
    }
}

std::string format_vertex_name(vertex* v){
    // make sure first character is not a number
    std::string s;
    size_t sz = 0;

    try {
        int i = (int)std::stod(v->name, &sz);

        switch(i){
            case 1:
                s += "one__";
                break;
            case 2:
                s += "two__";
                break;
            case 3:
                s += "three__";
                break;
            case 4:
                s += "four__";
                break;
            case 5:
                s += "five__";
                break;
            case 6:
                s += "six__";
                break;
            case 7:
                s += "seven__";
                break;
            case 8:
                s += "eight__";
                break;
            case 9:
                s += "nine__";
                break;
            case 10:
                s += "ten__";
                break;
            case 11:
                s += "eleven__";
                break;
            default:
                break;
        }
    }
    catch(std::invalid_argument ){}

    // if the name is just a numeric value, don't print it
    if (sz == v->name.length()){
        return "";
    }

    size_t pos = v->name.find("[");
    if (pos != std::string::npos)
        s += v->name.substr(sz, pos);
    else
        s += v->name.substr(sz);
    return s;
}

void digraph::print_vertex(vertex *v, std::ofstream &ofs, std::unordered_map<std::string, std::string> *obj_keys,
                           std::unordered_map<std::string, std::string> *eqn_keys) {

    const char* colors[] = {"black", "brown4", "darkorange3", "lightslateblue", "mediumorchid",
                            "burlywood4", "azure4", "darkorchid4", "aquamarine3", "olivedrab", "palevioletred", "darkgoldenrod2",
                            "gold4"};
    size_t cnt = eqn_keys->size() + obj_keys->size();


    for (size_t i = 0; i < v->edges_out.size(); i++){
        std::string src_str = format_vertex_name(v->edges_out[i]->src);
        std::string dest_str = format_vertex_name(v->edges_out[i]->dest);
        if (src_str.length() > 0 && dest_str.length() > 0){
            ofs << "\t" << src_str << " -> " << dest_str;
            edge* e = v->edges_out[i];
            if (e->type == 1){
                if (eqn_keys->find(e->obj_name) == eqn_keys->end()){
                    eqn_keys->insert({e->obj_name, colors[cnt]});
                    ofs << " [color=" << colors[cnt] << ", style=dashed]" << ";\n";
                    cnt+=1;
                }
                else{
                    ofs << " [color=" << eqn_keys->find(e->obj_name)->second << ", style=dashed]" << ";\n";
                }
            }
            else{
                if (obj_keys->find(e->obj_name) == obj_keys->end()){
                    obj_keys->insert({e->obj_name, colors[cnt]});
                    ofs << " [color=" << colors[cnt] << "]" << ";\n";
                    cnt+=1;
                }
                else{
                    ofs << " [color=" << obj_keys->find(e->obj_name)->second << "]" << ";\n";
                }
            }
        }
    }
}

void digraph::print_dot(std::string filepath) {

    std::string str = name;
    std::string::iterator end_pos = std::remove(str.begin(), str.end(), ' ');
    str.erase(end_pos, str.end());

    std::replace(str.begin(), str.end(), '-', '_');

    // setup print destination
    std::ofstream graph_file;
    std::ofstream legend_file;
    if (filepath.length() > 0){
        graph_file.open(filepath + "/" + str + ".gv");
        legend_file.open(filepath + "/" + str + "_legend.gv");
        assert(graph_file.is_open());
        assert(legend_file.is_open());

    } else{
        graph_file.copyfmt(std::cout);
        graph_file.clear(std::cout.rdstate());
        graph_file.basic_ios<char>::rdbuf(std::cout.rdbuf());
        legend_file.copyfmt(std::cout);
        legend_file.clear(std::cout.rdstate());
        legend_file.basic_ios<char>::rdbuf(std::cout.rdbuf());
    }


    // set up legend graph
    std::unordered_map<std::string, std::string> eqn_keys;
    std::unordered_map<std::string, std::string> obj_keys;

    // print graph of variables
    graph_file << "digraph " << str << " {\n;";
    graph_file << "\t" << "label =" << name <<";\n\tlabelloc=top;\n";
    graph_file << "\trankdir=LR;\n\tranksep=\"3\";\n";

    for (auto it = vertices.begin(); it != vertices.end(); ++it){
        if (vertex* v = it->second.at(0)){
            // make secondary cmods a different shape and color
            if (SAM_cmod_to_outputs.find(v->name) != SAM_cmod_to_outputs.end()){
                graph_file << "\t" << format_vertex_name(v) << " [shape=polygon, style=filled, fillcolor=darkslategray3]\n";
            }
        }
        // make nodes for ssc_variables colored
        if (vertex* v = it->second.at(1)){
            if (v->edges_out.size() + v->edges_in.size() > 0)
                graph_file << "\t" << format_vertex_name(v) << " [style=filled, fillcolor=grey]\n";
        }
    }
    graph_file << "\n";
    for (auto it = vertices.begin(); it != vertices.end(); ++it){
        for (size_t i = 0; i < 2; i++){
            if (vertex* v = it->second.at(i)){
                print_vertex(v, graph_file, &obj_keys, &eqn_keys);
            }
        }
    }
    graph_file << "}";
    graph_file.close();

    // print legend
    legend_file << "digraph " << str << "_legend {\n";
    legend_file << "\tlabel=\"Legend: " << name << "\";\n";
    legend_file << "\tlabel =" << name <<";\n\tlabelloc=top;\n";
    legend_file << "\tranksep=\"3\";\n";
    legend_file << "\tkey [label=<<table border=\"0\" cellpadding=\"2\" cellspacing=\"25\" cellborder=\"0\">\n";

    for (size_t i = 0; i < eqn_keys.size(); i++){
        legend_file << "\t<tr><td align=\"right\" port=\"e" << i << "\">eqn_var" << i  << "</td></tr>\n";
    }
    for (size_t i = 0; i < obj_keys.size(); i++){
        legend_file << "\t<tr><td align=\"right\" port=\"o" << i << "\">func_var" << i  << "</td></tr>\n";
    }

    legend_file << "\t</table>>]\n"
                 "\tkey2 [label=<<table border=\"0\" cellpadding=\"2\" cellspacing=\"25\" cellborder=\"0\">\n";

    for (size_t i = 0; i < eqn_keys.size(); i++){
        legend_file << "\t<tr><td port=\"e" << i << "\">&nbsp;</td></tr>\n";
    }
    for (size_t i = 0; i < obj_keys.size(); i++){
        legend_file << "\t<tr><td port=\"o" << i << "\">&nbsp;</td></tr>\n";
    }
    legend_file << "\t</table>>]\n";

    size_t i = 0;
    for (auto it = eqn_keys.begin(); it != eqn_keys.end(); ++it){
        legend_file << "\tkey:e" << i << ":e -> key2:e" << i << ":w [style=dashed, color=" << it->second;
        legend_file << ", label=\"" << it->first << "\"]\n";
        i++;
    }
    i=0;
    for (auto it = obj_keys.begin(); it != obj_keys.end(); ++it){
        legend_file << "\t\tkey:o" << i << ":e -> key2:o" << i << ":w [color=" << it->second;
        legend_file << ", label=\"" << it->first << "\"]\n";
        i++;
    }
    legend_file << "}";
    legend_file.close();

}