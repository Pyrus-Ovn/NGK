#include <iostream>

#include <restinio/all.hpp>

#include <json_dto/pub.hpp>

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
			& json_dto::mandatory( "ID"   		         , m_id            )
			& json_dto::mandatory( "Tidspunkt" 		     , m_dato          )
			& json_dto::mandatory( "Klokkeslaet"         , m_klokkeslaet   )
			& json_dto::mandatory( "Sted"				 , m_sted          )
			& json_dto::mandatory( "Temperatur"			 , m_temperatur    )  
			& json_dto::mandatory( "Luftfugtighed"		 , m_luftfugtighed );
	}

	std::string m_id;
	std::string m_dato;
	std::string m_klokkeslaet;
	sted_t m_sted;
	std::string m_temperatur;
	std::string m_luftfugtighed;
};

using vejrData_collection_t = std::vector<vejrData_t>;

namespace rr = restinio::router;

using router_t = rr::express_router_t<>;

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

private :
	vejrData_collection_t & m_vejrData;

	template < typename RESP >
	static RESP
	init_resp( RESP resp )
	{
		resp
			.append_header( "Server", "RESTinio sample server /v.0.6" )
			.append_header_date_field()
			.append_header( "Content-Type", "text/plain; charset=utf-8" );

		return resp;
	}

	template < typename RESP >
	static void
	mark_as_bad_request( RESP & resp )
	{
		resp.header().status_line( restinio::status_bad_request() );
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

	// Handlers for '/' path.
	router->http_get( "/", by( &vejrData_handler_t::vejrData_list ) );

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