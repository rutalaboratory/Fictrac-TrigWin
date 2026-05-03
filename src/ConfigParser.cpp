/// FicTrac http://rjdmoore.net/fictrac/
/// \file       ConfigParser.cpp
/// \brief      Read/write simple key/value pair config files.
/// \author     Richard Moore
/// \copyright  CC BY-NC-SA 3.0

#include "ConfigParser.h"

#include "Logger.h"
#include "fictrac_version.h"

#include <iostream>
#include <fstream>
#include <exception>    // try, catch
#include <algorithm>    // erase, remove
#include <iomanip>
#include <sstream>

using std::string;
using std::vector;

namespace {

string trim_copy(const string& value)
{
    const string whitespace = " \t\n\r";
    const std::size_t begin = value.find_first_not_of(whitespace);
    if (begin == string::npos) {
        return "";
    }
    const std::size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

template <typename NumberT>
bool parse_number_strict(const string& raw_value, NumberT& parsed_value, const char* type_name, const string& key)
{
    const string value = trim_copy(raw_value);
    std::size_t consumed = 0;
    try {
        if constexpr (std::is_same<NumberT, int>::value) {
            parsed_value = std::stoi(value, &consumed);
        }
        else {
            parsed_value = std::stod(value, &consumed);
        }
    }
    catch (std::exception& e) {
        LOG_ERR("Error parsing config file value (%s : %s) as %s! Error was: %s", key.c_str(), raw_value.c_str(), type_name, e.what());
        return false;
    }

    if (consumed != value.size()) {
        LOG_ERR("Error parsing config file value (%s : %s) as %s! Trailing content is not allowed.", key.c_str(), raw_value.c_str(), type_name);
        return false;
    }
    return true;
}

template <typename NumberT>
bool parse_flat_vector_strict(const string& raw_value, vector<NumberT>& parsed_values, const char* type_name, const string& key)
{
    const string value = trim_copy(raw_value);
    if ((value.size() < 2) || (value.front() != '{') || (value.back() != '}')) {
        LOG_ERR("Error parsing config file value (%s : %s) as %s vector! Expected outer braces.", key.c_str(), raw_value.c_str(), type_name);
        return false;
    }

    string inner = trim_copy(value.substr(1, value.size() - 2));
    parsed_values.clear();
    if (inner.empty()) {
        return true;
    }
    if ((inner.find('{') != string::npos) || (inner.find('}') != string::npos)) {
        LOG_ERR("Error parsing config file value (%s : %s) as %s vector! Nested braces are not allowed.", key.c_str(), raw_value.c_str(), type_name);
        return false;
    }

    std::replace(inner.begin(), inner.end(), ',', ' ');
    std::stringstream stream(inner);
    string token;
    while (stream >> token) {
        NumberT parsed_value{};
        if (!parse_number_strict(token, parsed_value, type_name, key)) {
            return false;
        }
        parsed_values.push_back(parsed_value);
    }
    return true;
}

} // namespace

///
/// Default constructor.
///
ConfigParser::ConfigParser()
{
}

///
/// Construct and parse given config file.
///
ConfigParser::ConfigParser(string fn)
{
    read(fn);
}

///
/// Default destructor.
///
ConfigParser::~ConfigParser()
{
}

///
/// Read in and parse specified config file.
///
int ConfigParser::read(string fn)
{
    //FIXME: replace corresponding keys rather than overwriting the whole file!

    LOG("Looking for config file: %s ..", fn.c_str());

    /// Open input file
    std::ifstream f(fn);
    if (!f.is_open()) {
        LOG_ERR("Could not open config file for reading!");
        return -1;
    }
    
    _fn = fn;   // save file name for debugging

    /// Parse to map
    string line;
    int line_number = 0;
    int error_count = 0;
    _data.clear();
    _comments.clear();
    while (getline(f,line)) {
        ++line_number;
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());

        const string whitespace = " \t\n";
        const std::size_t first_non_whitespace = line.find_first_not_of(whitespace);
        if (first_non_whitespace == string::npos) {
            continue;
        }

        if ((line[first_non_whitespace] == '#') || (line[first_non_whitespace] == '%')) {
            // save comment lines
            _comments.push_back(line);
            continue;
        }

        /// Tokenise
        std::size_t delim = line.find(":");
        if (delim >= line.size()) {
            LOG_ERR("Malformed config line %d in %s: expected key : value, got '%s'", line_number, fn.c_str(), line.c_str());
            error_count++;
            continue;
        }

        const std::size_t key_end = line.find_last_not_of(whitespace, delim == 0 ? 0 : delim - 1);
        if ((key_end == string::npos) || (key_end < first_non_whitespace)) {
            LOG_ERR("Malformed config line %d in %s: empty key before ':'", line_number, fn.c_str());
            error_count++;
            continue;
        }

        string key = line.substr(first_non_whitespace, key_end - first_non_whitespace + 1);
        string val = "";
        const std::size_t value_start = line.find_first_not_of(whitespace, delim + 1);
        if (value_start != string::npos) {
            val = line.substr(value_start);
        }

        if (_data.find(key) != _data.end()) {
            LOG_ERR("Duplicate config key (%s) at line %d in %s", key.c_str(), line_number, fn.c_str());
            error_count++;
            continue;
        }

        /// Add to map
        _data[key] = val;

        LOG_DBG("Extracted key: |%s|  val: |%s|", key.c_str(), val.c_str());
    }

    /// Clean up
    f.close();

    if (error_count > 0) {
        LOG_ERR("Config file parse failed (%d error(s)) in %s.", error_count, fn.c_str());
        return -1;
    }

    LOG("Config file parsed (%d key/value pairs).", _data.size());

    return static_cast<int>(_data.size());
}

///
/// Write specified map to file.
///
int ConfigParser::write(string fn)
{
    /// Open output file
    std::ofstream f(fn);
    if (!f.is_open()) {
        LOG_ERR("Could not open config file %s for writing!", fn.c_str());
        return -1;
    }
    
    /// Write header string
    f << "## FicTrac v" << FICTRAC_VERSION_MAJOR << "." << FICTRAC_VERSION_MIDDLE << "." << FICTRAC_VERSION_MINOR << " config file (build date " << __DATE__ << ")" << std::endl;

    /// Write map
    for (auto& it : _data) {
        f << std::left << std::setw(16) << it.first << " : " << it.second << std::endl;
        if (!f.good()) {
			LOG_ERR("Error writing key/value pair (%s : %s)!", it.first.c_str(), it.second.c_str());
            f.close();
            return -1;
        }
    }

    /// Write comments
    if (_comments.size() > 0) {
        f << std::endl;
        for (auto c : _comments) {
            f << c << std::endl;
        }
    }

    /// Clean up
    int nbytes = static_cast<int>(f.tellp());
    f.close();

	LOG_DBG("Wrote %d bytes to disk!", nbytes);

    return nbytes;
}

///
///
///
string ConfigParser::operator()(string key) const
{
    try {
        return _data.at(key);
    }
    catch (...) {
        LOG_DBG("Key (%s) not found.", key.c_str());
    }
    return "";
}

bool ConfigParser::hasKey(const std::string& key) const
{
    return _data.find(key) != _data.end();
}

///
/// Retrieve string value corresponding to specified key from map.
///
bool ConfigParser::getStr(string key, string& val) {
    auto it = _data.find(key);
    if (it != _data.end()) {
        val = _data[key];
        return true;
    }
    LOG_DBG("Key (%s) not found.", key.c_str());
    return false;
}

///
/// Retrieve int value corresponding to specified key from map.
///
bool ConfigParser::getInt(string key, int& val) {
    string str;
    if (getStr(key, str)) {
        return parse_number_strict(str, val, "INT", key);
    }
    return false;
}

///
/// Retrieve double value corresponding to specified key from map.
///
bool ConfigParser::getDbl(string key, double& val) {
    string str;
    if (getStr(key, str)) {
        return parse_number_strict(str, val, "DBL", key);
    }
    return false;
}

///
/// Retrieve bool value corresponding to specified key from map.
///
bool ConfigParser::getBool(string key, bool& val) {
    string str;
    if (getStr(key, str)) {
        str = trim_copy(str);
        if (!str.compare("Y") || !str.compare("y") || !str.compare("1")) {
            val = true;
            return true;
        }
        else if (!str.compare("N") || !str.compare("n") || !str.compare("0")) {
            val = false;
            return true;
        }
        else {
            LOG_ERR("Error parsing config file value (%s : %s) as BOOL!", key.c_str(), str.c_str());
        }
    }
    return false;
}

///
/// Retrieve vector of int values corresponding to specified key from map.
///
bool ConfigParser::getVecInt(std::string key, vector<int>& val) {
    string str;
    if (getStr(key, str)) {
        return parse_flat_vector_strict(str, val, "INT", key);
    }
    return false;
}

///
/// Retrieve vector of double values corresponding to specified key from map.
///
bool ConfigParser::getVecDbl(std::string key, vector<double>& val) {
    string str;
    if (getStr(key, str)) {
        return parse_flat_vector_strict(str, val, "DBL", key);
    }
    return false;
}

///
/// Retrieve vector of vector of int values corresponding to specified key from map.
///
bool ConfigParser::getVVecInt(std::string key, vector<vector<int> >& val) {
    string str;
    if (getStr(key, str)) {
        const string value = trim_copy(str);
        val.clear();

        if ((value.size() < 2) || (value.front() != '{') || (value.back() != '}')) {
            LOG_ERR("Error parsing config file value (%s : %s) as INT matrix! Expected outer braces.", key.c_str(), str.c_str());
            return false;
        }

        const string inner = value.substr(1, value.size() - 2);
        std::size_t index = 0;
        while (index < inner.size()) {
            while ((index < inner.size()) && ((inner[index] == ' ') || (inner[index] == '\t') || (inner[index] == '\n') || (inner[index] == ','))) {
                ++index;
            }
            if (index >= inner.size()) {
                break;
            }
            if (inner[index] != '{') {
                LOG_ERR("Error parsing config file value (%s : %s) as INT matrix! Expected nested braces.", key.c_str(), str.c_str());
                return false;
            }

            const std::size_t close = inner.find('}', index + 1);
            if (close == string::npos) {
                LOG_ERR("Error parsing config file value (%s : %s) as INT matrix! Missing closing brace.", key.c_str(), str.c_str());
                return false;
            }

            vector<int> poly;
            if (!parse_flat_vector_strict("{" + inner.substr(index + 1, close - index - 1) + "}", poly, "INT", key)) {
                return false;
            }
            val.push_back(poly);
            index = close + 1;
        }
        return true;
    }
    return false;
}

///
/// Print all key/value pairs to stdout.
///
void ConfigParser::printAll()
{
    LOG_DBG("Config file (%s):\n", _fn.c_str());
    
    std::stringstream s;
    for (auto& it : _data) {
        s << "\t" << it.first << "\t: " << it.second << std::endl;
    }
    LOG_DBG("%s", s.str().c_str());
}
