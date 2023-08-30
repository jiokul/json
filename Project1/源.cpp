#include <iostream>
#include <variant>
#include <vector>
#include <map>
#include <optional>
#include <string>
#include <fstream>
#include <sstream>
#include <string_view>

namespace json{
	using Bool = bool;
	using Int = int64_t;
	using Float = double;
	using String = std::string;
	using Null = std::monostate;
	struct Node;
	using Array = std::vector<Node>;
	using Object = std::map<std::string, Node>;
	using Value = std::variant<Null, Bool, Int, Float, String, Object, Array>;
	struct Node {
		Value value;
		Node(Value _value):value(_value){}
		Node() :value(Null{}) {}//为什么monostate后接{}
		auto& operator[](const std::string& key) {//重载[]
			if (auto object = std::get_if<Object>(&value)){
				return (*object)[key];
			}
			throw std::runtime_error("not an object");
		}
		auto operator[](size_t index){//同重载[]，输入不同,size_t表示c中最大长度
			if (auto array = std::get_if<Array>(&value)) {
				return array->at(index);
			}
			throw std::runtime_error("not an array");
		}
		void push(const Node& rhs) {//推入数据
			if (auto array = std::get_if<Array>(&value)) {
				array->push_back(rhs);
			}
		}
	};

	struct JsonParser {//词法分析器
		std::string_view json_str;//只读字符串，节省空间内存
		size_t pos = 0;
		void parser_whitespace() {//判断json中是否有空格
			while(pos< json_str.size()&&isspace(json_str[pos])){
				pos++;
			}
		}

		auto parse_null()->std::optional<Value> {//判断json中是否存在空值
			if (json_str.substr(pos, 4) == "null")
			{
				pos += 4;
				return Null{};
			}
			return {};
		}
		auto parse_true()->std::optional<Value> {
			if (json_str.substr(pos, 4) == "true")
			{
				pos += 4;
				return true;
			}
			return {};
		}
		auto parse_flase()->std::optional<Value> {
			if (json_str.substr(pos, 5) == "false")
			{
				pos += 5;
				return false;
			}
			return std::nullopt;
		}

		auto parse_number()->std::optional<Value> {//判断是否为数字
			size_t endpos = pos;
			while (endpos < json_str.size() && (std::isdigit(json_str[endpos]) || json_str[endpos] == 'e' || json_str[endpos] == '.')) {
				endpos++;
			}//跳过.与e（1e-4）
			std::string number = std::string{ json_str.substr(pos, endpos) };//截取数字部分
			pos = endpos;
			static auto is_Float = [](std::string& number) {//使用上述重载操作[]，number作为key查找键对应值
				return number.find('.') != number.npos || number.find('e') != number.npos;//如果find未找到，则会返回npos
			};
			if (is_Float(number)) {
				try {
					Float ret = std::stod(number);//返回double
					return ret;
				}
				catch (...) {
					return {};
				}
			}
			else {
				try {
					Int ret = std::stoi(number);//返回Int
					return ret;
				}
				catch (...) {
					return {};
				}
			}
		}

		auto parse_string()->std::optional<Value> {
			pos++;//跳过"
			size_t endpos = pos;
			while (endpos < json_str.size() && json_str[endpos] != '\"')
			{
				endpos++;
			}
			std::string str = std::string{ json_str.substr(pos,endpos - pos) };
			pos = ++endpos;//跳过"
			return str;
		}

		auto parse_array()->std::optional<Value> {
			pos++;//跳过[
			Array arr;
			while (pos < json_str.size() && json_str[pos] != ']')
			{
				auto value = parse_value();
				arr.push_back(value.value());
				parser_whitespace();
				if (pos < json_str.size() && json_str[pos] == ',') {
					pos++;
				}
				parser_whitespace();
			}
			pos++;//跳过]
			return arr;
		}

		auto parse_object()->std::optional<Value> {
			pos++;//跳过{
			Object obj;
			while (pos < json_str.size() && json_str[pos] != '}')
			{
				auto key = parse_value();
				parser_whitespace();
				if (!std::holds_alternative<String>(key.value())) {
					return {};
				}
				if (pos < json_str.size() && json_str[pos] == ':')
				{
					pos++;//跳过:
				}
				parser_whitespace();
				auto val = parse_value();
				obj[std::get<String>(key.value())] = val.value();
				parser_whitespace();
				if (pos < json_str.size() && json_str[pos] == ',') {
					pos++;
				}
				parser_whitespace();
			}
			pos++;//跳过}
			return obj;
		}

		auto parse_value()->std::optional<Value> {
			parser_whitespace();
			switch (json_str[pos]) {
			case 'n'://null
				return parse_null();
			case 't'://true
				return parse_true();
			case 'f'://false
				return parse_flase();
			case '\"'://"string"
				return parse_string();
			case '['://[1,2,1]
				return parse_array();
			case '{'://{}
				return parse_object();
			default ://number
				return parse_number();
			}
		}

		auto parse()->std::optional<Node> {
			parser_whitespace();
			auto val = parse_value();
			if (!val) {
				return std::nullopt;
			}
			return Node(*val);
		}
	};

	auto parser(std::string_view json_str) ->std::optional<Node> {
		JsonParser p{ json_str };
		return p.parse();
	}

	class JsonGenerator {
	public:
		static auto generate(const Node& node)->std::string {
			return std::visit(
				[](auto&& arg)->std::string {
					using T = std::decay_t<decltype(arg)>;
					if constexpr (std::is_same_v<T, Null>) {
						return "null";
					}
					else if constexpr (std::is_same_v<T, Bool>) {
						return arg ? "true" : "false";
					}
					else if constexpr (std::is_same_v<T, Int>) {
						return std::to_string(arg);
					}
					else if constexpr (std::is_same_v<T, Float>) {
						return std::to_string(arg);
					}
					else if constexpr (std::is_same_v<T, String>) {
						return generate_string(arg);
					}
					else if constexpr (std::is_same_v<T, Array>) {
						return generate_array(arg);
					}
					else if constexpr (std::is_same_v<T, Object>) {
						return generate_object(arg);
					}
				},
				node.value);
		}
		static auto generate_string(const String& string)->std::string {
			std::string json_str = "\"";
			json_str += string;
			json_str += '"';
			return json_str;
		}
		static auto generate_array(const Array& array)->std::string {
			std::string json_str = "[";
			for (const auto& node : array)
			{
				json_str += generate(node);
				json_str += ",";
			}
			json_str += "]";
			return json_str;
		}
		static auto generate_object(const Object& obj)->std::string {
			std::string json_str = "{";
			for (const auto& [key, node] : obj) {
				json_str += generate_string(key);
				json_str += ":";
				json_str += generate(node);
				json_str += ",";
			}
			if(!obj.empty()) json_str.pop_back();//删除最后一个元素？？？
			json_str += "}";
			return json_str;
		}
	};

	inline auto generate(const Node& node) -> std::string { return JsonGenerator::generate(node); }

	auto  operator << (std::ostream& out, const Node& t) ->std::ostream& {
		out << JsonGenerator::generate(t);
		return out;
	}

}

using namespace json;


int main() {
	std::ifstream fin("2.json");
	std::stringstream ss; ss << fin.rdbuf();
	std::string s{ ss.str() };
	auto x = parser(s).value();
	std::cout << x << "\n";
	//x["configurations"].push({ true });
	//x["configurations"].push({ Null {} });
	//x["version"] = { 114514LL };
	//std::cout << x << "\n";
}