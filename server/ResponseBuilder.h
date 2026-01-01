#ifndef RESPONSEBUILDER_H
#define RESPONSEBUILDER_H

#include <string>
#include <map>
#include <sstream>
#include <iostream>
using namespace std;

class ResponseBuilder {
public:
    
    string success(const string& status, 
                       const map<string, string>& data = {}) {
        ostringstream json;
        
        json << "{";
        json << "\"status\":\"success\",";
        json << "\"code\":\"" << status << "\"";
        
        if (!data.empty()) {
            json << ",\"data\":{";
            
            bool first = true;
            for (const auto& pair : data) {
                if (!first) json << ",";
                json << "\"" << pair.first << "\":\"" << pair.second << "\"";
                first = false;
            }
            
            json << "}";
        }
        
        json << "}";
        
        return json.str();
    }
    
    string error(const string& code, const string& message) {
        ostringstream json;
        
        json << "{";
        json << "\"status\":\"error\",";
        json << "\"code\":\"" << code << "\",";
        json << "\"message\":\"" << escape_json(message) << "\"";
        json << "}";
        
        return json.str();
    }
    
    string list(const string& status, int count, 
                    const string& message = "") {
        ostringstream json;
        
        json << "{";
        json << "\"status\":\"success\",";
        json << "\"code\":\"" << status << "\",";
        json << "\"count\":" << count;
        
        if (!message.empty()) {
            json << ",\"message\":\"" << escape_json(message) << "\"";
        }
        
        json << "}";
        
        return json.str();
    }
    
private:
    string escape_json(const string& str) {
        string result;
        result.reserve(str.length());
        
        for (char c : str) {
            switch (c) {
                case '\"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c; break;
            }
        }
        
        return result;
    }
};

#endif