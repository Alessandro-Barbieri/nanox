/*************************************************************************************/
/*      Copyright 2009-2018 Barcelona Supercomputing Center                          */
/*                                                                                   */
/*      This file is part of the NANOS++ library.                                    */
/*                                                                                   */
/*      NANOS++ is free software: you can redistribute it and/or modify              */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      NANOS++ is distributed in the hope that it will be useful,                   */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with NANOS++.  If not, see <https://www.gnu.org/licenses/>.            */
/*************************************************************************************/

#include <vector>
#include <stdint.h>
#include <string>
#include "atomic.hpp"
#include "lock.hpp"
#include <tr1/unordered_map>
#include "papi.h"

namespace nanos {

    struct Node;

    int used_edge_types[5] = {0, 0, 0, 0, 0};

    enum DependencyType {
        Null,
        True,
        Anti,
        Output,
        InConcurrent,
        OutConcurrent,
        InCommutative,
        OutCommutative,
        InAny,
        OutAny
    };

    enum EdgeKind{
        Nesting,
        Synchronization,
        Dependency
    };

    struct Edge {
        // Class members
        EdgeKind _kind;
        DependencyType _dep_type;
        Node* _source;
        Node* _target;
        std::pair<void*, void*> _data_range;    // Overlapping region of memory

        // Constructor
        Edge(EdgeKind kind, DependencyType dep_type, Node* source, Node* target, std::pair<void*, void*> data_range = std::pair<void*, void*>(NULL, NULL)) :
            _kind(kind),
            _dep_type(dep_type),
            _source(source),
            _target(target),
            _data_range(data_range)
        {}

        Node* get_source() const {return _source;}
        Node* get_target() const {return _target;}
        EdgeKind get_kind() const {return _kind;}
        DependencyType get_dependency_type() const {return _dep_type;}
        std::pair<void*, void*> get_data_range() const {return _data_range;}

        uint64_t get_data_size() const {
            if(_data_range.first == NULL || _data_range.second == NULL) {
                return 0;
            }
            return ((uint64_t)_data_range.second - (uint64_t)_data_range.first) + 1;
        }

        bool is_nesting() const {
           return _kind == Nesting;
        }

        bool is_synchronization() const {
           return _kind == Synchronization;
        }

        bool is_dependency() const {
           return _kind == Dependency;
        }

        bool is_true_dependency( ) const {
            return ( ( _kind == Dependency ) &&
                     ( ( _dep_type == True ) || ( _dep_type == InConcurrent ) || ( _dep_type == InCommutative ) || ( _dep_type == InAny ) ) );
        }

        bool is_anti_dependency( ) const {
            return ( ( _kind == Dependency ) && ( _dep_type == Anti ) );
        }

        bool is_output_dependency( ) const {
            return ( ( _kind == Dependency ) &&
                     ( ( _dep_type == Output ) || ( _dep_type == OutConcurrent ) || ( _dep_type == OutCommutative ) || ( _dep_type == OutAny ) ) );
        }

        bool is_concurrent_dep( ) const {
            return ( ( _kind == Dependency ) &&
                     ( ( _dep_type == InConcurrent ) || ( _dep_type == OutConcurrent ) ) );
        }

        bool is_commutative_dep( ) const {
            return ( ( _kind == Dependency ) &&
                     ( ( _dep_type == InCommutative ) || ( _dep_type == OutCommutative ) ) );
        }

        bool is_any_dep( ) const {
            return ( ( _kind == Dependency ) &&
                     ( ( _dep_type == InAny ) || ( _dep_type == OutAny ) ) );
        }
    };

    inline bool operator==(Edge const& lhs, Edge const& rhs) {
        return (lhs._kind == rhs._kind &&
                lhs._dep_type == rhs._dep_type &&
                lhs._source == rhs._source &&
                lhs._target == rhs._target &&
                lhs._data_range == rhs._data_range);
    }

    enum NodeType {
        Root,
        BarrierNode,
        ConcurrentNode,
        CommutativeNode,
        TaskNode,
        TaskwaitNode
    };

    struct NodeIO;

    struct Node {
        // Class members
        int64_t _wd_id;
        int64_t _func_id;
        NodeType _type;
        std::vector<Edge*> _entry_edges;
        std::vector<Edge*> _exit_edges;
        double _total_time;
        double _last_time;
        Lock _entry_lock;
        Lock _exit_lock;

        bool _printed;
        bool _critical;

        int _papi_event_set;
        std::vector<std::pair<int, long long> > _papi_counters;   // first - event id, second - counter value

        std::vector<NodeIO> _io;

        // Constructor
        Node( int64_t wd_id, int64_t func_id, NodeType type )
            : _wd_id( wd_id ), _func_id( func_id ), _type( type ),
              _entry_edges( ), _exit_edges( ),
              _total_time( 0.0 ), _last_time( 0.0 ), _printed( false ), _critical( false ),
              _papi_event_set( PAPI_NULL ), _papi_counters( )
        {}

        int64_t get_wd_id() const {return _wd_id;}
        int64_t get_funct_id() const {return _func_id;}
        std::vector<Edge*> const &get_entries() {return _entry_edges;}
        std::vector<Edge*> const &get_exits() {return _exit_edges;}
        double get_last_time() const {return _last_time;}
        void set_last_time(double time) {_last_time = time;}
        double get_total_time() const {return _total_time;}
        std::vector<std::pair<int, long long> > get_perf_counters() const {return _papi_counters;}
        std::vector<NodeIO> get_io() const {return _io;}

        void add_io(NodeIO const& io) {
            _io.push_back(io);
        }

        void add_total_time( double time ) {
           _total_time += time;
        }

        Node* get_parent_task( ) {
            Node* res = NULL;
            _entry_lock.acquire();
            for( std::vector<Edge*>::const_iterator it = _entry_edges.begin( ); it != _entry_edges.end( ); ++it ) {
                if( (*it)->is_nesting( ) ) {
                    res = (*it)->get_source( );
                    break;
                }
            }
            _entry_lock.release();
            return res;
        }

        //called in finalize, and connect_nodes (with _exit_lock acquired)
        bool is_connected_with( Node* target ) const {
            bool res = false;
            for( std::vector<Edge*>::const_iterator it = _exit_edges.begin( ); it != _exit_edges.end( ); ++it ) {
                if( (*it)->get_target( ) == target ) {
                    res = true;
                    break;
                }
            }
            return res;
        }

        //called in connect_nodes (with _exit_lock acquired)
        std::vector<Edge*> get_connections( Node* target ) const {
            std::vector<Edge*> result;
            for( std::vector<Edge*>::const_iterator it = _exit_edges.begin( ); it != _exit_edges.end( ); ++it ) {
                if( (*it)->get_target( ) == target ) {
                    result.push_back(*it);
                }
            }
            return result;
        }

        //only called in finalize
        bool is_previous_synchronized( ) const {
            bool res = false;
            for( std::vector<Edge*>::const_iterator it = _entry_edges.begin( ); it != _entry_edges.end( ); ++it ) {
                if( (*it)->is_dependency( ) || (*it)->is_synchronization( ) ) {
                    res = true;
                    break;
                }
            }
            return res;
        }

        bool is_next_synchronized( ) {
            bool res = false;
            _exit_lock.acquire();
            for( std::vector<Edge*>::const_iterator it = _exit_edges.begin( ); it != _exit_edges.end( ); ++it ) {
                if( (*it)->is_dependency( ) || (*it)->is_synchronization( ) ) {
                    res = true;
                    break;
                }
            }
            _exit_lock.release();
            return res;
        }

        //! Only connect the nodes if they are not previously connected or
        //! the new type of connection is different from the existing one
        static void connect_nodes(
            Node* source, Node* target,
            EdgeKind kind,
            void* data_start = NULL, void* data_end = NULL,
            DependencyType dep_type = Null )
        {
            source->_exit_lock.acquire();

            Edge* new_edge = new Edge( kind, dep_type, source, target, std::pair<void*, void*>(data_start, data_end) );

            // If the edge is the same, don't connect it again
            std::vector<Edge*> c = source->get_connections(target);
            for (std::vector<Edge*>::iterator it = c.begin(); it != c.end(); ++it) {
                if(*(*it) == *new_edge) {
                    source->_exit_lock.release();
                    return;
                }
            }

            source->_exit_edges.push_back( new_edge );
            target->_entry_lock.acquire();
            target->_entry_edges.push_back( new_edge );
            target->_entry_lock.release();

            // store the edge type as used
            switch (kind)
            {
                case Nesting :          used_edge_types[3] = 1; break;
                case Synchronization :  used_edge_types[0] = 1; break;
                case Dependency :       {
                                            switch (dep_type)
                                            {
                                                case True:      used_edge_types[0] = 1; break;
                                                case Anti:      used_edge_types[1] = 1; break;
                                                case Output:    used_edge_types[2] = 1; break;
                                                default:        break;
                                            };
                                            break;
                                        };
                default:                break;
            };
            if (source->is_critical() && target->is_critical())
            {
                used_edge_types[4] = 1;
            }

            source->_exit_lock.release();
        }

        bool is_task( ) const {
            return _type == TaskNode;
        }

        bool is_taskwait( ) const {
            return _type == TaskwaitNode;
        }

        bool is_barrier( ) const {
            return _type == BarrierNode;
        }

        bool is_concurrent( ) const {
            return _type == ConcurrentNode;
        }

        bool is_commutative( ) const {
            return _type == CommutativeNode;
        }

        bool is_printed( ) const {
            return _printed;
        }

        void set_printed( ) {
            _printed = true;
        }

        bool is_critical( ) {
           return _critical;
        }

        void set_critical( ) {
           _critical = true;
        }

        void start_operation_counters(std::vector<int>& papi_event_codes)
        {
            int rc;

            if(_papi_event_set == PAPI_NULL) {
                if((rc = PAPI_create_eventset(&_papi_event_set)) != PAPI_OK) {
                    std::cerr << "Failed to create node event set. ";
                    std::cerr << "Papi error: (" << rc << ") - " << PAPI_strerror(rc) << "\n";
                }

                std::vector<int>::iterator it;
                for(it = papi_event_codes.begin(); it != papi_event_codes.end(); ++it) {
                    if((rc = PAPI_add_event(_papi_event_set, *it)) != PAPI_OK) {
                        std::cerr << "Failed to add event with id " << *it << " to node event set. ";
                        std::cerr << "Papi error: (" << rc << ") - " << PAPI_strerror(rc) << "\n";
                    } else {
                        _papi_counters.push_back(std::make_pair(*it, 0));
                    }
                }
            }

            if((rc = PAPI_start(_papi_event_set)) != PAPI_OK) {
                std::cerr << "Failed to start node performance counters. ";
                std::cerr << "Papi error: (" << rc << ") - " << PAPI_strerror(rc) << "\n";
            }
        }

        void suspend_operation_counters(bool const last) {
            int rc;
            long long counter_values[_papi_counters.size()];

            if((rc = PAPI_stop(_papi_event_set, counter_values)) != PAPI_OK) {
                std::cerr << "Failed to get node performance counters. ";
                std::cerr << "Papi error: (" << rc << ") - " << PAPI_strerror(rc) << "\n";
            } else {
                for(unsigned i = 0; i < _papi_counters.size(); i++) {
                    _papi_counters[i].second += counter_values[i];
                }
            }

            if(last) {
                if((rc = PAPI_cleanup_eventset(_papi_event_set)) != PAPI_OK) {
                    std::cerr << "Failed to clean up node event set. ";
                    std::cerr << "Papi error: (" << rc << ") - " << PAPI_strerror(rc) << ".\n";
                } else {
                    _papi_event_set = PAPI_NULL;
                }
            }
        }
    };


    std::string node_colors[] = {
        "aliceblue", "antiquewhite", "antiquewhite1", "antiquewhite2", "antiquewhite3",
        "antiquewhite4", "aquamarine", "aquamarine1", "aquamarine2", "aquamarine3",
        "aquamarine4", "azure", "azure1", "azure2", "azure3",
        "azure4", "beige", "bisque", "bisque1", "bisque2",
        "bisque3", "bisque4", "black", "blanchedalmond", "blue",
        "blue1", "blue2", "blue3", "blue4", "blueviolet",
        "brown", "brown1", "brown2", "brown3", "brown4",
        "burlywood", "burlywood1", "burlywood2", "burlywood3", "burlywood4",
        "cadetblue", "cadetblue1", "cadetblue2", "cadetblue3", "cadetblue4",
        "chartreuse", "chartreuse1", "chartreuse2", "chartreuse3", "chartreuse4",
        "chocolate", "chocolate1", "chocolate2", "chocolate3", "chocolate4",
        "coral", "coral1", "coral2", "coral3", "coral4",
        "cornflowerblue", "cornsilk", "cornsilk1", "cornsilk2", "cornsilk3",
        "cornsilk4", "crimson", "cyan", "cyan1", "cyan2",
        "cyan3", "cyan4", "darkgoldenrod", "darkgoldenrod1", "darkgoldenrod2",
        "darkgoldenrod3", "darkgoldenrod4", "darkgreen", "darkkhaki", "darkolivegreen",
        "darkolivegreen1", "darkolivegreen2", "darkolivegreen3", "darkolivegreen4", "darkorange",
        "darkorange1", "darkorange2", "darkorange3", "darkorange4", "darkorchid",
        "darkorchid1", "darkorchid2", "darkorchid3", "darkorchid4", "darksalmon",
        "darkseagreen", "darkseagreen1", "darkseagreen2", "darkseagreen3", "darkseagreen4",
        "darkslateblue", "darkslategray", "darkslategray1", "darkslategray2", "darkslategray3",
        "darkslategray4", "darkslategrey", "darkturquoise", "darkviolet", "deeppink",
        "deeppink1", "deeppink2", "deeppink3", "deeppink4", "deepskyblue",
        "deepskyblue1", "deepskyblue2", "deepskyblue3", "deepskyblue4", "dimgray",
        "dimgrey", "dodgerblue", "dodgerblue1", "dodgerblue2", "dodgerblue3",
        "dodgerblue4", "firebrick", "firebrick1", "firebrick2", "firebrick3",
        "firebrick4", "floralwhite", "forestgreen", "gainsboro", "ghostwhite",
        "gold", "gold1", "gold2", "gold3", "gold4",
        "goldenrod", "goldenrod1", "goldenrod2", "goldenrod3", "goldenrod4",
        "gray", "gray0", "gray1", "gray10", "gray100",
        "gray11", "gray12", "gray13", "gray14", "gray15",
        "gray16", "gray17", "gray18", "gray19", "gray2",
        "gray20", "gray21", "gray22", "gray23", "gray24",
        "gray25", "gray26", "gray27", "gray28", "gray29",
        "gray3", "gray30", "gray31", "gray32", "gray33",
        "gray34", "gray35", "gray36", "gray37", "gray38",
        "gray39", "gray4", "gray40", "gray41", "gray42",
        "gray43", "gray44", "gray45", "gray46", "gray47",
        "gray48", "gray49", "gray5", "gray50", "gray51",
        "gray52", "gray53", "gray54", "gray55", "gray56",
        "gray57", "gray58", "gray59", "gray6", "gray60",
        "gray61", "gray62", "gray63", "gray64", "gray65",
        "gray66", "gray67", "gray68", "gray69", "gray7",
        "gray70", "gray71", "gray72", "gray73", "gray74",
        "gray75", "gray76", "gray77", "gray78", "gray79",
        "gray8", "gray80", "gray81", "gray82", "gray83",
        "gray84", "gray85", "gray86", "gray87", "gray88",
        "gray89", "gray9 ", "gray90", "gray91", "gray92",
        "gray93", "gray94", "gray95", "gray96", "gray97",
        "gray98", "gray99", "green", "green1", "green2",
        "green3", "green4", "greenyellow", "grey", "grey0",
        "grey1", "grey10", "grey100", "grey11", "grey12",
        "grey13", "grey14", "grey15", "grey16", "grey17",
        "grey18", "grey19", "grey2", "grey20", "grey21",
        "grey22", "grey23", "grey24", "grey25", "grey26",
        "grey27", "grey28", "grey29", "grey3", "grey30",
        "grey31", "grey32", "grey33", "grey34", "grey35",
        "grey36", "grey37", "grey38", "grey39", "grey4",
        "grey40", "grey41", "grey42", "grey43", "grey44",
        "grey45", "grey46", "grey47", "grey48", "grey49",
        "grey5", "grey50", "grey51", "grey52", "grey53",
        "grey54", "grey55", "grey56", "grey57", "grey58",
        "grey59", "grey6", "grey60", "grey61", "grey62",
        "grey63", "grey64", "grey65", "grey66", "grey67",
        "grey68", "grey69", "grey7", "grey70", "grey71",
        "grey72", "grey73", "grey74", "grey75", "grey76",
        "grey77", "grey78", "grey79", "grey8", "grey80",
        "grey81", "grey82", "grey83", "grey84", "grey85",
        "grey86", "grey87", "grey88", "grey89", "grey9",
        "grey90", "grey91", "grey92", "grey93", "grey94",
        "grey95", "grey96", "grey97", "grey98", "grey99",
        "honeydew", "honeydew1", "honeydew2", "honeydew3", "honeydew4",
        "hotpink", "hotpink1", "hotpink2", "hotpink3", "hotpink4",
        "indianred", "indianred1", "indianred2", "indianred3", "indianred4",
        "indigo", "invis", "ivory", "ivory1", "ivory2",
        "ivory3", "ivory4", "khaki", "khaki1", "khaki2",
        "khaki3", "khaki4", "lavender", "lavenderblush", "lavenderblush1",
        "lavenderblush2", "lavenderblush3", "lavenderblush4", "lawngreen", "lemonchiffon",
        "lemonchiffon1", "lemonchiffon2", "lemonchiffon3", "lemonchiffon4", "lightblue",
        "lightblue1", "lightblue2", "lightblue3", "lightblue4", "lightcoral",
        "lightcyan", "lightcyan1", "lightcyan2", "lightcyan3", "lightcyan4",
        "lightgoldenrod", "lightgoldenrod1", "lightgoldenrod2", "lightgoldenrod3", "lightgoldenrod4",
        "lightgoldenrodyellow", "lightgray", "lightgrey", "lightpink", "lightpink1",
        "lightpink2", "lightpink3", "lightpink4", "lightsalmon", "lightsalmon1",
        "lightsalmon2", "lightsalmon3", "lightsalmon4", "lightseagreen", "lightskyblue",
        "lightskyblue1", "lightskyblue2", "lightskyblue3", "lightskyblue4", "lightslateblue",
        "lightslategray", "lightslategrey", "lightsteelblue", "lightsteelblue1", "lightsteelblue2",
        "lightsteelblue3", "lightsteelblue4", "lightyellow", "lightyellow1", "lightyellow2",
        "lightyellow3", "lightyellow4", "limegreen", "linen", "magenta",
        "magenta1", "magenta2", "magenta3", "magenta4", "maroon",
        "maroon1", "maroon2", "maroon3", "maroon4", "mediumaquamarine",
        "mediumblue", "mediumorchid", "mediumorchid1", "mediumorchid2", "mediumorchid3",
        "mediumorchid4", "mediumpurple", "mediumpurple1", "mediumpurple2", "mediumpurple3",
        "mediumpurple4", "mediumseagreen", "mediumslateblue", "mediumspringgreen", "mediumturquoise",
        "mediumvioletred", "midnightblue", "mintcream", "mistyrose", "mistyrose1",
        "mistyrose2", "mistyrose3", "mistyrose4", "moccasin", "navajowhite",
        "navajowhite1", "navajowhite2", "navajowhite3", "navajowhite4", "navy",
        "navyblue", "none", "oldlace", "olivedrab", "olivedrab1",
        "olivedrab2", "olivedrab3", "olivedrab4", "orange", "orange1",
        "orange2", "orange3", "orange4", "orangered", "orangered1",
        "orangered2", "orangered3", "orangered4", "orchid", "orchid1",
        "orchid2", "orchid3", "orchid4", "palegoldenrod", "palegreen",
        "palegreen1", "palegreen2", "palegreen3", "palegreen4", "paleturquoise",
        "paleturquoise1", "paleturquoise2", "paleturquoise3", "paleturquoise4", "palevioletred",
        "palevioletred1", "palevioletred2", "palevioletred3", "palevioletred4", "papayawhip",
        "peachpuff", "peachpuff1", "peachpuff2", "peachpuff3", "peachpuff4",
        "peru", "pink", "pink1", "pink2", "pink3",
        "pink4", "plum", "plum1", "plum2", "plum3",
        "plum4", "powderblue", "purple", "purple1", "purple2",
        "purple3", "purple4", "red", "red1", "red2",
        "red3", "red4", "rosybrown", "rosybrown1", "rosybrown2",
        "rosybrown3", "rosybrown4", "royalblue", "royalblue1", "royalblue2",
        "royalblue3", "royalblue4", "saddlebrown", "salmon", "salmon1",
        "salmon2", "salmon3", "salmon4", "sandybrown", "seagreen",
        "seagreen1", "seagreen2", "seagreen3", "seagreen4", "seashell",
        "seashell1", "seashell2", "seashell3", "seashell4", "sienna",
        "sienna1", "sienna2", "sienna3", "sienna4", "skyblue",
        "skyblue1", "skyblue2", "skyblue3", "skyblue4", "slateblue",
        "slateblue1", "slateblue2", "slateblue3", "slateblue4", "slategray",
        "slategray1", "slategray2", "slategray3", "slategray4", "slategrey",
        "snow", "snow1", "snow2", "snow3", "snow4",
        "springgreen", "springgreen1", "springgreen2", "springgreen3", "springgreen4",
        "steelblue", "steelblue1", "steelblue2", "steelblue3", "steelblue4",
        "tan", "tan1", "tan2", "tan3", "tan4",
        "thistle", "thistle1", "thistle2", "thistle3", "thistle4",
        "tomato", "tomato1", "tomato2", "tomato3", "tomato4",
        "transparent", "turquoise", "turquoise1", "turquoise2", "turquoise3",
        "turquoise4", "violet", "violetred", "violetred1", "violetred2",
        "violetred3", "violetred4", "wheat", "wheat1", "wheat2",
        "wheat3", "wheat4", "white", "whitesmoke", "yellow",
        "yellow1", "yellow2", "yellow3", "yellow4", "yellowgreen"
    };

    inline std::string &wd_to_color_hash(std::string description)
    {
        int const node_color_count = sizeof(node_colors) / sizeof(node_colors[0]);
        std::tr1::hash<std::string> hash_fn;
        return node_colors[ hash_fn(description) % node_color_count ];
    }

    inline std::string formatSize(long long bytes) {
        std::string units[] = {"B","kB","MB","GB","TB","PB","EB","YB"};
        int const max_units = sizeof(units) / sizeof(units[0]);

        double size = bytes;
        int i;
        for(i = 0; i < max_units && size > 1024; i++) {
            size /= 1024;
        }

        std::stringstream ss;
        ss.precision(3);
        ss << size << units[i];
        return ss.str();
    }

    inline std::string formatTime(long long us) {
        std::string units[] = {"us","ms","S","M","H","D"};
        int const unit_multiples[] = {1000, 1000, 60, 60, 24};
        int const max_units = sizeof(units) / sizeof(units[0]);

        double time = us;
        int i;
        for(i = 0; i < max_units && time > unit_multiples[i]; i++) {
            time /= unit_multiples[i];
        }

        std::stringstream ss;
        ss.precision(3);
        ss << time << units[i];
        return ss.str();
    }

    template<class T_value>
    inline void printJsonAttribute(
        std::string const indent,
        std::string const key,
        T_value const value,
        std::ostream& os)
    {
        os << indent << "\"" << key << "\": " << value;
    }

    // Overload for strings
    inline void printJsonAttribute(
        std::string const indent,
        std::string const key,
        std::string const value,
        std::ostream& os)
    {
        os << indent << "\"" << key << "\": \"" << value << "\"";
    }

    // Overload for c strings
    inline void printJsonAttribute(
        std::string const indent,
        std::string const key,
        char const* value,
        std::ostream& os)
    {
        os << indent << "\"" << key << "\": \"" << value << "\"";
    }

    // Overload for boolean
    inline void printJsonAttribute(
        std::string const indent,
        std::string const key,
        bool const value,
        std::ostream& os)
    {
        os << std::boolalpha;
        os << indent << "\"" << key << "\": " << value;
        os << std::noboolalpha;
    }

    inline void printJsonNullAttribute(
        std::string const indent,
        std::string const key,
        std::ostream& os)
    {
        os << indent << "\"" << key << "\": null";
    }

    template<class T_value>
    inline void printJsonAttributeArray(
        std::string const indent,
        std::string const name,
        std::vector<std::pair<std::string, T_value> > const data,
        std::ostream& os)
    {
        os << indent << "\"" << name << "\": {\n";

        for(unsigned i = 0; i < data.size(); i++) {
            printJsonAttribute(indent + "  ", data[i].first, data[i].second, os);
            if(i < data.size() - 1) {
                os << ",\n";
            }
        }

        os << "\n" << indent << "}";
    }


    // Struct used to describe a task input or output
    struct NodeIO {
        bool const _is_input;
        bool const _is_output;
        void const * const _start_address;
        void const * const _end_address;
        uint64_t const _size;

        NodeIO(nanos_data_access_t const& data_access) :
            _is_input(data_access.isInput()),
            _is_output(data_access.isOutput()),
            _start_address(data_access.getDepAddress()),
            _end_address((void*)((uint64_t)_start_address + (data_access.getSize() - 1))),
            _size(data_access.getSize())
        {}

        void to_json(std::string const indent, std::ostream& os) {
            os << indent << "{\n";
            printJsonAttribute(indent + "  ", "is_input", _is_input, os);
            os << ",\n";
            printJsonAttribute(indent + "  ", "is_output", _is_output, os);
            os << ",\n";
            printJsonAttribute(indent + "  ", "start_address", (uint64_t)_start_address, os);
            os << ",\n";
            printJsonAttribute(indent + "  ", "end_address", (uint64_t)_end_address, os);
            os << ",\n";
            printJsonAttribute(indent + "  ", "size", (uint64_t)_size, os);
            os << "\n" << indent << "}";
        }

        std::string to_json(std::string const indent) {
            std::stringstream ss;
            to_json(indent, ss);
            return ss.str();
        }
    };

} // namespace nanos
