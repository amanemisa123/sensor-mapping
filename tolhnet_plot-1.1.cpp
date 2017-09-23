#include <iostream>
#include <sstream>
#include <locale>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <map>
#include <limits>
#include <functional>
#include <unistd.h>
#include <time.h>
#include <sqlite3.h>
#include <iomanip>
using namespace std;


#define MAX_POST_LEN 1024


const map<string, pair<string,string>> sensorNames {
	// html name -> (sensor name first part, sensor title)
	{"sensor01", {"N1:0", "1"}},
	{"sensor02", {"N1:1", "2"}},
	{"sensor03", {"N1:2", "3"}},
	{"sensor04", {"N1:3", "4"}},
	{"sensor05", {"N1:4", "5"}},
	{"sensor06", {"N1:5", "6"}},
	{"sensor07", {"N1:6", "7"}},

	{"sensor08", {"N2:0", "8"}},
	{"sensor09", {"N2:1", "9"}},
	{"sensor10", {"N2:2", "10"}},
	{"sensor11", {"N2:3", "11"}},
	{"sensor12", {"N2:4", "12"}},
	{"sensor13", {"N2:5", "13"}},
	{"sensor14", {"N2:6", "14"}},

	{"sensor15", {"N5:0", "15"}},
	{"sensor16", {"N5:1", "16"}},
	{"sensor17", {"N5:2", "17"}},
	{"sensor18", {"N5:3", "18"}},
	{"sensor19", {"N5:4", "19"}},
	{"sensor20", {"N5:5", "20"}},
};


const map<string, pair<string,string>> sensorParams {
	// html name -> (sensor name last part, plot title)
	{"t", {"IRAmb", "Temperature"}},
	{"h", {"HumHum", "Humidity"}},
	{"p", {"PrePre", "Pressure"}},
};



vector<string> split(string const &s, char delim) {
	vector<string> result;
	string token;
	for (char c: s) {
		if (c == delim) {
			if (token.size()) result.push_back(token);
			token.clear();
		}
		else token += c;
	}
	if (token.size()) result.push_back(token);
	return result;
}


int returnError(string const &msg) {
	cout << "Content-type: text/plain\n\n";
	cout << "ERROR: " << msg << endl;
	return 1;
}


class Cleaning: public vector<function<void()>> {
	// class to clean things up, all the contained function objects
	//  are executed at destruction
public:
	~Cleaning() {
		for (auto f: *this) f();
	}
};

int timeconvert(string date){
	std::string rep =":00:00";
	
	date.replace(13,6,rep); 

	std::tm t = {};
	std::istringstream ss(date);
	int c;
	if (ss >> std::get_time(&t, "%Y-%m-%dT%H:%M:%S"))
    	{        	
                c = std::mktime(&t);
		cout << c << ".\n";
    	}
   	 else
    	{
        	std::cout << "Parse failed\n";
    	}


return c;
}

int main() {
	//{
	//	char *s = getenv("CONTENT_LENGTH");
	//	if (!s) return returnError("CONTENT_LENGTH not set");
	//	int postLen = atoi(s);
	//	if (postLen <= 0 || postLen > MAX_POST_LEN)
	//		return returnError("data size");
	//}

	string postData;
	getline(cin, postData);
	map<string, string> postValues;
	for (auto &s: split(postData, '&')) {
		auto v = split(s, '=');		
		if (v.size() == 2) postValues.insert({v[0], v[1]});
	}

	string paramName, paramLabel;
	{
		auto p = sensorParams.find(postValues["parametername"]);
		if (p == sensorParams.end())
			return returnError("wrong parameter name");
		paramName = p->second.first;
		paramLabel = p->second.second;
	}

	string initDate = postValues["iDate"].c_str();
	string stopDate = postValues["sDate"].c_str();
	
	int timestart = timeconvert(initDate);
	int timestop = timeconvert(stopDate);

	if (timestop >= timestart) 
		{
		return returnError("invalid date");
		}
	//int lastHours = atoi(postValues["lasthours"].c_str());
	//if (lastHours < 1) lastHours = 24;

	vector<pair<string, string>> plotValues;  // (sensor title, parameter name)
	for (auto const &v: postValues)
		if (v.second == "on") {
			auto i = sensorNames.find(v.first);
			//cout << i->second.second << ".\n";
			if (i == sensorNames.end()) continue;
			plotValues.push_back({i->second.second, i->second.first + ":" + paramName});
		}

//	cout << "Content-type: text/plain\n\n";
//	for (auto &v: plotValues) cout << v.first << "  " << v.second << endl << endl;

	// read data from database and generate data file for gnuplot, one section per sensor
	sqlite3 *dbMeasures = nullptr;
	if (sqlite3_open_v2("/usr/lib/cgi-bin/tolhnet/sensor.db", &dbMeasures, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
		return returnError(string("cannot open datatabase: ") + sqlite3_errmsg(dbMeasures));
	// temporary files for data and plot commands
	char tmpPlotData[] = "/usr/lib/cgi-bin/tolhnet/plotData_XXXXXX", tmpPlotCmd[] = "/usr/lib/cgi-bin/tolhnet/plotCmd_XXXXXX";
	FILE *fPlotData = fdopen(mkstemp(tmpPlotData), "w"),
		*fPlotCmd = fdopen(mkstemp(tmpPlotCmd), "w");
	Cleaning cleaning;
	cleaning.push_back([tmpPlotData](){ unlink(tmpPlotData); });  // delete file at the end
	cleaning.push_back([tmpPlotCmd](){ unlink(tmpPlotCmd); });  // delete file at the end

	// exec queries
	sqlite3_stmt *query;
	sqlite3_prepare_v2(dbMeasures,
		"select t/1000 as tepoch, measure from measures "
		"where name = :name and t/1000 >= :tmin and t/1000 <= :tmax and measure != 0 "
		"order by t",
		-1, &query, nullptr);
	int id_tmin = sqlite3_bind_parameter_index(query, ":tmin"), id_tmax = sqlite3_bind_parameter_index(query, ":tmax"),
		id_name = sqlite3_bind_parameter_index(query, ":name");
	int numSections = 0;
	for (auto const &value: plotValues) {
		sqlite3_reset(query);
		sqlite3_bind_int64(query, id_tmin, timestart);
		//sqlite3_bind_int64(query, id_tmin, time(nullptr) - lastHours * 3600);
		sqlite3_bind_int64(query, id_tmax, timestop);
		//sqlite3_bind_int64(query, id_tmax, numeric_limits<int64_t>::max());

		sqlite3_bind_text(query, id_name, value.second.c_str(), -1, SQLITE_STATIC);
		if (sqlite3_step(query) == SQLITE_ROW) {
			++numSections;
			fprintf(fPlotData, "\"date\"\t\"%s\"\n", value.first.c_str());
			do {
				fprintf(fPlotData, "%lld\t%f\n", sqlite3_column_int64(query, 0), sqlite3_column_double(query, 1));
			}
			while (sqlite3_step(query) == SQLITE_ROW);
			fprintf(fPlotData, "\n\n");
		}
	}
	fclose(fPlotData);
	sqlite3_finalize(query);
	sqlite3_close(dbMeasures);

	// stupid gnuplot always assumes UTC timezone when converting an epoch time
	// to human-readable fields, so we compute the offset for our timezone and
	// add it explicitly to the x values when plotting
	long timezoneOffset;
	{
		time_t t = time(NULL);
		struct tm lt;
		localtime_r(&t, &lt);
		timezoneOffset = lt.tm_gmtoff;
	}

	// write command file for gnuplot
	fprintf(fPlotCmd, "set terminal svg size 800,600\n");
	fprintf(fPlotCmd, "set object rectangle from screen 0,0 to screen 1,1 behind fillcolor rgb 'white' fillstyle solid noborder\n");
	//fprintf(fPlotCmd, "set output \"%s\"\n", "/usr/lib/cgi-bin/tolhnet/plot.svg");
	fprintf(fPlotCmd, "set title \"%s, %d hours\"\nset key autotitle columnheader\nset style data lines\nset xdata time\n",
		paramLabel.c_str(), lastHours);
	fprintf(fPlotCmd, "set timefmt \"%%s\"\nset format x \"%s\"\n",
		(lastHours > 72 ? "%b %d" :"%H:%M"));
	fprintf(fPlotCmd, "\nplot \\\n");
	for (int i = 0; i < numSections; ++i) {
		fprintf(fPlotCmd, "'%s' i %d u ($1 + %ld):2", tmpPlotData, i, timezoneOffset);
		if (i != (numSections - 1)) fprintf(fPlotCmd, ",\\\n");
		else fprintf(fPlotCmd, "\n");
	}
	fclose(fPlotCmd);
	if (!numSections) return returnError("no data found");

	// call gnuplot and capture output
	string cmdLine("gnuplot ");
	cmdLine += tmpPlotCmd;
	FILE *fp = popen(cmdLine.c_str(), "r");
	if (!fp) return returnError("error invoking gnuplot");
	printf("Content-type: image/svg+xml\n\n");
	uint8_t binData[1024];
	size_t n;
	while ((n = fread(binData, 1, sizeof binData, fp)))
		fwrite(binData, 1, n, stdout);
	pclose(fp);
}
