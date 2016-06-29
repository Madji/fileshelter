#include <iostream>

#include <Wt/WServer>

#include "utils/Config.hpp"
#include "utils/Logger.hpp"

#include "database/DbHandler.hpp"
#include "database/DbUpdater.hpp"

#include "ui/FileShelterApplication.hpp"

static std::vector<std::string> getWtArgs(std::string path)
{
	std::vector<std::string> args;

	args.push_back(path);
	args.push_back("-c" + Config::instance().getString("wt-config"));
	args.push_back("--docroot=" + Config::instance().getString("docroot"));
	args.push_back("--approot=" + Config::instance().getString("approot"));

	if (Config::instance().getBool("tls-enable", false))
	{
		args.push_back("--https-port=" + std::to_string( Config::instance().getULong("listen-port", 5081)));
		args.push_back("--https-address=" + Config::instance().getString("listen-addr", "0.0.0.0"));
		args.push_back("--ssl-certificate=" + Config::instance().getString("tls-cert"));
		args.push_back("--ssl-private-key=" + Config::instance().getString("tls-key"));
		args.push_back("--ssl-tmp-dh=" + Config::instance().getString("tls-dh"));
	}
	else
	{
		args.push_back("--http-port=" + std::to_string( Config::instance().getULong("listen-port", 5081)));
		args.push_back("--http-address=" + Config::instance().getString("listen-addr", "0.0.0.0"));
	}

	return args;
}

int main(int argc, char *argv[])
{

	boost::filesystem::path configFilePath = "/etc/fileshelter.conf";
	int res = EXIT_FAILURE;

	assert(argc > 0);
	assert(argv[0] != NULL);

	if (argc >= 2)
		configFilePath = std::string(argv[1], 0, 256);

	try
	{
		Config::instance().setFile(configFilePath);

		std::vector<std::string> wtArgs = getWtArgs(argv[0]);

		// Construct argc/argv for Wt
		const char* wtArgv[wtArgs.size()];
		for (std::size_t i = 0; i < wtArgs.size(); ++i)
			wtArgv[i] = wtArgs[i].c_str();

		Wt::WServer server(argv[0]);
		server.setServerConfiguration (wtArgs.size(), const_cast<char**>(wtArgv));

		Wt::WServer::instance()->logger().configure("*"); // log everything, TODO configure this

		// Make sure working directory exists
		boost::filesystem::create_directories(Config::instance().getString("working-dir"));

		// Initializing a connection pool to the database that will be shared along services
		std::unique_ptr<Wt::Dbo::SqlConnectionPool>
			connectionPool( Database::Handler::createConnectionPool(Config::instance().getString("working-dir") + "/fileshelter.db"));

//		Database::Updater& dbUpdater = Database::Updater::instance();
//		dbUpdater.setConnectionPool(*connectionPool);

		// bind entry point
		server.addEntryPoint(Wt::Application, boost::bind(UserInterface::FileShelterApplication::create,
					_1, boost::ref(*connectionPool)));

		// Start
		FS_LOG(MAIN, INFO) << "Starting database updater...";
//		dbUpdater.start();

		FS_LOG(MAIN, INFO) << "Starting server...";
		server.start();

		// Wait
		FS_LOG(MAIN, INFO) << "Now running...";
		Wt::WServer::waitForShutdown(argv[0]);

		// Stop
		FS_LOG(MAIN, INFO) << "Stopping server...";
		server.stop();

		FS_LOG(MAIN, INFO) << "Stopping database updater...";
//		dbUpdater.stop();

		res = EXIT_SUCCESS;
	}
	catch( libconfig::FileIOException& e)
	{
		std::cerr << "Cannot open config file " << configFilePath << std::endl;
	}
	catch( libconfig::ParseException& e)
	{
		std::cerr << "Caught libconfig::ParseException! error='" << e.getError() << "', file = '" << e.getFile() << "', line = " << e.getLine() << std::endl;
	}
	catch(Wt::WServer::Exception& e)
	{
		std::cerr << "Caught a WServer::Exception: " << e.what() << std::endl;
	}
	catch(std::exception& e)
	{
		std::cerr << "Caught std::exception: " << e.what() << std::endl;
	}

	return res;
}
