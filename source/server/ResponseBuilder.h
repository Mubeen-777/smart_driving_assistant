#ifndef RESPONSEBUILDER_H
#define RESPONSEBUILDER_H

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>
using namespace std;

class ResponseBuilder {
public:
    
    string success(const string& status, const map<string, string>& data = {}) {
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

    string success_with_array(const string& status,
                              const string& array_key,
                              const vector<map<string, string>>& items) {
        ostringstream json;
        
        json << "{";
        json << "\"status\":\"success\",";
        json << "\"code\":\"" << status << "\",";
        json << "\"data\":{";
        json << "\"" << array_key << "\":[";
        
        bool first_item = true;
        for (const auto& item : items) {
            if (!first_item) json << ",";
            json << "{";
            
            bool first_field = true;
            for (const auto& field : item) {
                if (!first_field) json << ",";
                json << "\"" << field.first << "\":\"" << escape_json(field.second) << "\"";
                first_field = false;
            }
            
            json << "}";
            first_item = false;
        }
        
        json << "],\"count\":" << items.size();
        json << "}";
        json << "}";
        
        return json.str();
    }
    
private:
    string escape_json(const string& str) {
        ostringstream result;
        for (unsigned char c : str) {
            switch (c) {
                case '\"': result << "\\\""; break;
                case '\\': result << "\\\\"; break;
                case '\b': result << "\\b"; break;
                case '\f': result << "\\f"; break;
                case '\n': result << "\\n"; break;
                case '\r': result << "\\r"; break;
                case '\t': result << "\\t"; break;
                default:
                    if (c < 32) {
                        result << "\\u" << hex << setfill('0') << setw(4) << (int)c;
                    } else {
                        result << c;
                    }
                    break;
            }
        }
        return result.str();
    }
};

#endif