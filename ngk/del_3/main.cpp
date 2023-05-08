#include <iostream>

#include <restinio/all.hpp>

#include <json_dto/pub.hpp>

#include <restinio/websocket/websocket.hpp>

#include <fstream>

//struct for placement in weather data
struct sted_t 
{
	sted_t() = default;
	
	sted_t(std::string Navn, std::string Lat, std::string Lon) 
			: m_Navn{std::move( Navn ) }
			, m_Lat{std::move( Lat )}
		    , m_Lon{std::move( Lon )}
	{}

	template<typename JSON_IO>
	void
	json_io(JSON_IO &io)
	{
		io
			& json_dto::mandatory( "Navn", m_Navn )
			& json_dto::mandatory( "Lat"	  , m_Lat 		)
			& json_dto::mandatory( "Lon"	  , m_Lon 		);
	}	

	std::string m_Navn;
	std::string m_Lat;
	std::string m_Lon;
};

struct vejrData_t 
{
	vejrData_t() = default;
	
	vejrData_t(std::string id, std::string dato, std::string klokkeslaet, 
					sted_t sted, std::string temperatur, std::string luftfugtighed)
		:	m_id{std::move( id ) } 	
		,   m_dato{std::move( dato ) }
		,   m_klokkeslaet{std::move( klokkeslaet ) }
		,	m_sted{std::move( sted ) }
		, 	m_temperatur{std::move( temperatur ) }
		,	m_luftfugtighed{std::move( luftfugtighed ) }
	{}

	template<typename JSON_IO>
	void json_io(JSON_IO &io)
	{
		io
			& json_dto::mandatory( "id"   		         , m_id            )
			& json_dto::mandatory( "dato" 		     , m_dato          )
			& json_dto::mandatory( "klokkeslaet"         , m_klokkeslaet   )
			& json_dto::mandatory( "sted"				 , m_sted          )
			& json_dto::mandatory( "temperatur"			 , m_temperatur    )  
			& json_dto::mandatory( "luftfugtighed"		 , m_luftfugtighed );
	}

	std::string m_id;
	std::string m_dato;
	std::string m_klokkeslaet;
	sted_t      m_sted;
	std::string m_temperatur;
	std::string m_luftfugtighed;
};

using vejrData_collection_t = std::vector<vejrData_t>;

namespace rr = restinio::router;

using router_t = rr::express_router_t<>;

//web socket
namespace rws = restinio::websocket::basic;
using traits_t =
    restinio::traits_t<restinio::asio_timer_manager_t,
                       restinio::single_threaded_ostream_logger_t, router_t>;
using ws_registry_t = std::map<std::uint64_t, rws::ws_handle_t>;

class vejrData_handler_t
{
public :
	explicit vejrData_handler_t( vejrData_collection_t &vejrData )
		:	m_vejrData( vejrData )
	{}

	vejrData_handler_t( const vejrData_handler_t & ) = delete;
	vejrData_handler_t( vejrData_handler_t && ) = delete;

	auto vejrData_list(const restinio::request_handle_t& req, rr::route_params_t ) const
	{
		auto resp = init_resp( req->create_response() );

		resp.set_body(json_dto::to_json(m_vejrData));

		return resp.done();
	}

  	auto options(restinio::request_handle_t req, restinio::router::route_params_t)
  	{
    	auto resp = init_resp(req->create_response());
		resp.append_header(restinio::http_field::access_control_allow_methods, "*");
    	resp.append_header(restinio::http_field::access_control_allow_headers,
                       "content-type");
    	resp.append_header(restinio::http_field::access_control_max_age, "86400");
    	return resp.done();
  	}

/*
del 2
*/

//delopgave 1 - create new entry
auto on_vejrData_new_entry(const restinio::request_handle_t& req, rr::route_params_t)
{
	auto resp = init_resp(req->create_response());

	try
	{
		// Add the data to the collection.
		m_vejrData.emplace_back(json_dto::from_json<vejrData_t>(req->body()));
	}
	catch( const std::exception & /*ex*/ )
	{
		// error handling
		mark_as_bad_request(resp);
	}

	return resp.done();
}

  /*
  delopgave 2 
  */

  //fetch the three latest entries
  auto on_vejrData_get_three(const restinio::request_handle_t& req, rr::route_params_t) const        
    {                                                                                               
    	auto resp = init_resp(req->create_response());                                              
                                                                                                        
        resp.set_body("");                                  
                                                                                                        
        const auto & vd = m_vejrData;                                                           

        if (vd.size() > 3)                                                                          
        {                                                                                           
        	for (std::size_t i = vd.size(); i > vd.size()-3;i--)                                    
            {                                                                                       
            	resp.append_body(json_dto::to_json<vejrData_t>(vd[i-1]));                       
            }                                                                                       
        }                                                                                           
        else                                                                                        
        {                                                                                           
        	resp.append_body("Fejl - Der er mindre end 3 datapunkter lagret!");                                 
		}

        return resp.done();                                                                         
    }

	// function to get all entries:
auto on_vejrData_get_all(const restinio::request_handle_t& req, rr::route_params_t) const
{
	// Init
	auto resp = init_resp(req->create_response());

	// bind to vector
	const auto & vd = m_vejrData;

	// Set the body
	resp.set_body(json_dto::to_json< std::vector<vejrData_t> >(vd));

	// Return the HTTP response.
	return resp.done();
}

// function to get entries from date:
auto on_vejrData_get_from_date(const restinio::request_handle_t& req, rr::route_params_t params)
{
	// fetch a date
	const auto vejrDataDate = restinio::cast_to<std::string>(params["dato"]);

	// Init
	auto resp = init_resp(req->create_response());

	// Set the body
	resp.set_body("");

	// Init counter and error handling
	int cnt = 0;
	bool found_match = false;

	// Iterate over the all elements in vejrData
	for(auto i = m_vejrData.begin(); i != m_vejrData.end(); i++)
	{
		// If the current element's date matches `vejrDataDate`, append
		if(i->m_dato == vejrDataDate)
		{
			resp.append_body(json_dto::to_json<vejrData_t>(m_vejrData[cnt]));
			found_match = true;
		}
		++cnt;
	}

		// error handling
	if (!found_match)
	{
		resp.append_body("Ingen datapunkter for dato = " + vejrDataDate);
	}

	return resp.done();
}

/*
Delopgave 3
*/

//function to update entries based on id
auto on_id_updateData(const restinio::request_handle_t& req, rr::route_params_t params)
	{
		const auto vejrDataID = restinio::cast_to<std::uint32_t>(params["vejrDataID"]);

		//init
		auto resp = init_resp(req->create_response());

		try
		{
			auto vd = json_dto::from_json<vejrData_t>(req->body());

			if(0 != vejrDataID && vejrDataID <= m_vejrData.size())
			{
				// Update data for ID
				m_vejrData[vejrDataID - 1] = vd;
			}
			else
			{
				//error handling
				mark_as_bad_request(resp);
				resp.set_body("Ingen vejr data for id nummer = " + std::to_string(vejrDataID) + "\n");
			}
		}
		catch( const std::exception & /*ex*/ )
		{
			//error parsing JSON = bad request
			mark_as_bad_request(resp);
		}

		return resp.done();
	}

/*
del 3
*/

//web socket CORS
auto on_live_update(const restinio::request_handle_t &req,
                      rr::route_params_t                params)
  {
    // check if the request is an upgrade connection type aka webSocket
    if (restinio::http_connection_header_t::upgrade ==
        req->header().connection())
    {
      // create webSocket handler
      auto wsh = rws::upgrade<traits_t>(
          *req, rws::activation_t::immediate,
          [this](auto wsh, auto m)
          {
            if (rws::opcode_t::text_frame == m->opcode() ||
                rws::opcode_t::binary_frame == m->opcode() ||
                rws::opcode_t::continuation_frame == m->opcode())
            {
              wsh->send_message(*m);
            }
            else if (rws::opcode_t::ping_frame == m->opcode())
            {
              auto resp = *m;
              resp.set_opcode(rws::opcode_t::pong_frame);
              wsh->send_message(resp);
            }
            else if (rws::opcode_t::connection_close_frame == m->opcode())
            {
              m_registry.erase(wsh->connection_id());
            }
          });
      m_registry.emplace(wsh->connection_id(), wsh);
      init_resp(req->create_response()).done();
      return restinio::request_accepted();
    }
    return restinio::request_rejected();
  }

private :
	vejrData_collection_t & m_vejrData;

	template < typename RESP >
	static RESP
	init_resp( RESP resp )
	{
		resp
			.append_header( "Server", "RESTinio sample server /v.0.6" )
			.append_header_date_field()
			.append_header( "Content-Type", "text/plain; charset=utf-8" )
			.append_header(restinio::http_field::access_control_allow_origin, "*");

		return resp;
	}

	template < typename RESP >
	static void
	mark_as_bad_request( RESP & resp )
	{
		resp.header().status_line( restinio::status_bad_request() );
	}

	//web socket
	  ws_registry_t m_registry;

  void sendMessage(std::string message)
  {
    for (auto [k, v] : m_registry)
      v->send_message(rws::final_frame, rws::opcode_t::text_frame, message);
  }
};

auto server_handler( vejrData_collection_t & vejrData_collection )
{
	auto router = std::make_unique< router_t >();
	auto handler = std::make_shared< vejrData_handler_t >( std::ref(vejrData_collection) );

	auto by = [&]( auto method ) {
		using namespace std::placeholders;
		return std::bind( method, handler, _1, _2 );
	};

	auto method_not_allowed = []( const auto & req, auto ) {
			return req->create_response( restinio::status_method_not_allowed() )
					.connection_close()
					.done();
		};

	router->add_handler(restinio::http_method_options(), "/",
                      by(&vejrData_handler_t::options));

	//method from del 1
	router->http_get( "/", by( &vejrData_handler_t::vejrData_list ) );

	//methods from del 2
  	router->http_post("/", by(&vejrData_handler_t::on_vejrData_new_entry));
  	router->http_get("/getThreeLatest", by(&vejrData_handler_t::on_vejrData_get_three));
  	router->http_get("/getFromDate/:dato", by(&vejrData_handler_t::on_vejrData_get_from_date));
	router->http_put("/id/:vejrDataID", by(&vejrData_handler_t::on_id_updateData));

	//web socket
	router->add_handler(restinio::http_method_options(), "/id/:vejrDataID", by(&vejrData_handler_t::options));
	router->http_get("/chat", by(&vejrData_handler_t::on_live_update));


	// Disable all other methods for '/'.
	router->add_handler(
			restinio::router::none_of_methods(
					restinio::http_method_get(), restinio::http_method_post() ),
			"/", method_not_allowed );

	return router;
}

int main()
{
	using namespace std::chrono;

	try
	{
		using traits_t =
			restinio::traits_t<
				restinio::asio_timer_manager_t,
				restinio::single_threaded_ostream_logger_t,
				router_t >;

		vejrData_collection_t vejrData_collection{
        {"1",
         "20211105",
         "12:15",
         {"Aarhus N", "13.692", "19.438"},
         "13.1",
         "70%"},
    };

		restinio::run(
			restinio::on_this_thread< traits_t >()
				.address( "localhost" )
				.request_handler( server_handler( vejrData_collection ) )
				.read_next_http_message_timelimit( 10s )
				.write_http_response_timelimit( 1s )
				.handle_request_timeout( 1s ) );
	}
	catch( const std::exception & ex )
	{
		std::cerr << "Error: " << ex.what() << std::endl;
		return 1;
	}

	return 0;
}