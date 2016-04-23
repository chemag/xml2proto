// Compile with:
//   clang++ -Wall -lprotobuf -lprotoc -lxml2 -I/usr/include/libxml2 -lpthread ./xml2proto.cc

#include <iostream>
#include <string>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/compiler/importer.h>
#include <libxml/xmlreader.h>

using namespace std;
using namespace google::protobuf;
using namespace google::protobuf::compiler;

// Why libxml doesn't define this itself is beyond me.
enum XmlNodeType {
  XML_NODE_NONE                  =  0,
  XML_NODE_ELEMENT               =  1,
  XML_NODE_ATTRIBUTE             =  2,
  XML_NODE_TEXT                  =  3,
  XML_NODE_CDATA                 =  4,
  XML_NODE_ENTITYREFERENCE       =  5,
  XML_NODE_ENTITY                =  6,
  XML_NODE_PROCESSINGINSTRUCTION =  7,
  XML_NODE_COMMENT               =  8,
  XML_NODE_DOCUMENT              =  9,
  XML_NODE_DOCUMENTTYPE          = 10,
  XML_NODE_DOCUMENTFRAGMENT      = 11,
  XML_NODE_NOTATION              = 12,
  XML_NODE_WHITESPACE            = 13,
  XML_NODE_SIGNIFICANTWHITESPACE = 14,
  XML_NODE_ENDELEMENT            = 15,
  XML_NODE_ENDENTITY             = 16,
  XML_NODE_XMLDECLARATION        = 17
};

const char* NodeTypeName(int node_type) {
  switch (node_type) {
    case XML_NODE_NONE                 : return "XML_NODE_NONE";
    case XML_NODE_ELEMENT              : return "XML_NODE_ELEMENT";
    case XML_NODE_ATTRIBUTE            : return "XML_NODE_ATTRIBUTE";
    case XML_NODE_TEXT                 : return "XML_NODE_TEXT";
    case XML_NODE_CDATA                : return "XML_NODE_CDATA";
    case XML_NODE_ENTITYREFERENCE      : return "XML_NODE_ENTITYREFERENCE";
    case XML_NODE_ENTITY               : return "XML_NODE_ENTITY";
    case XML_NODE_PROCESSINGINSTRUCTION: return "XML_NODE_PROCESSINGINSTRUCTION";
    case XML_NODE_COMMENT              : return "XML_NODE_COMMENT";
    case XML_NODE_DOCUMENT             : return "XML_NODE_DOCUMENT";
    case XML_NODE_DOCUMENTTYPE         : return "XML_NODE_DOCUMENTTYPE";
    case XML_NODE_DOCUMENTFRAGMENT     : return "XML_NODE_DOCUMENTFRAGMENT";
    case XML_NODE_NOTATION             : return "XML_NODE_NOTATION";
    case XML_NODE_WHITESPACE           : return "XML_NODE_WHITESPACE";
    case XML_NODE_SIGNIFICANTWHITESPACE: return "XML_NODE_SIGNIFICANTWHITESPACE";
    case XML_NODE_ENDELEMENT           : return "XML_NODE_ENDELEMENT";
    case XML_NODE_ENDENTITY            : return "XML_NODE_ENDENTITY";
    case XML_NODE_XMLDECLARATION       : return "XML_NODE_XMLDECLARATION";
    default: return NULL;
  }
}

inline string ToString(const xmlChar* text) {
  return text ? string(reinterpret_cast<const char*>(text)) : "NULL";
}

void FormatEnumValue(string* str) {
  for (string::iterator i = str->begin(); i != str->end(); ++i) {
    if (isalnum(*i)) {
      *i = toupper(*i);
    } else {
      *i = '_';
    }
  }
}

bool ParseSimpleField(Message *message,
                      const Message::Reflection* reflection,
                      const FieldDescriptor* field,
                      const string& value) {
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32: {
      char* endptr = NULL;
      if (field->label() == FieldDescriptor::LABEL_REPEATED) {
        reflection->AddInt32(message, field, strtol(value.c_str(), &endptr, 0));
      } else {
        reflection->SetInt32(message, field, strtol(value.c_str(), &endptr, 0));
      }
      if (*endptr != '\0') {
        cerr << "Expected int value for " << field->full_name()
             << " but got \"" << value << "\"." << endl;
        return false;
      }
      return true;
    }

    case FieldDescriptor::CPPTYPE_BOOL: {
      bool bool_value;
      if (value == "true" || value == "yes") {
        bool_value = true;
      } else if (value == "false" || value == "no") {
        bool_value = false;
      } else {
        cerr << "Expected bool value for " << field->full_name()
             << " but got \"" << value << "\"." << endl;
        return false;
      }

      if (field->label() == FieldDescriptor::LABEL_REPEATED) {
        reflection->AddBool(message, field, bool_value);
      } else {
        reflection->SetBool(message, field, bool_value);
      }
      return true;
    }

    case FieldDescriptor::CPPTYPE_STRING:
      if (field->label() == FieldDescriptor::LABEL_REPEATED) {
        reflection->AddString(message, field, value);
      } else {
        reflection->SetString(message, field, value);
      }
      return true;

    case FieldDescriptor::CPPTYPE_ENUM: {
      string enum_value_name = field->enum_type()->name();
      enum_value_name += '_';
      enum_value_name += value;
      FormatEnumValue(&enum_value_name);
      const EnumValueDescriptor* enum_value =
        field->enum_type()->FindValueByName(enum_value_name);

      if (enum_value == NULL) {
        cerr << field->enum_type()->full_name() << " has no value named \""
             << enum_value_name << "\".";
        return false;
      }

      if (field->label() == FieldDescriptor::LABEL_REPEATED) {
        reflection->AddEnum(message, field, enum_value);
      } else {
        reflection->SetEnum(message, field, enum_value);
      }
      return true;
    }

    default:
      cerr << "Don't know how to parse simple field: " << field->full_name()
           << endl;
      return false;
  }
}

struct FieldRef {
  Message* message;
  const FieldDescriptor* field;
};

FieldRef FindField(Message* base, const string& name) {
  FieldRef result;
  result.message = base;
  result.field = NULL;
  const Descriptor* descriptor = base->GetDescriptor();
  const Message::Reflection* reflection = base->GetReflection();

  result.field = descriptor->FindFieldByName("element");
  if (result.field != NULL &&
      result.field->type() == FieldDescriptor::TYPE_MESSAGE &&
      result.field->label() == FieldDescriptor::LABEL_REPEATED) {
    result.message = reflection->AddMessage(base, result.field);
    descriptor = result.message->GetDescriptor();
    reflection = result.message->GetReflection();
  }

  while (true) {
    result.field = descriptor->FindFieldByName(name);
    if (result.field != NULL) return result;  // found it

    // Check for "more".
    result.field = descriptor->FindFieldByName("more");
    if (result.field == NULL ||
        result.field->type() != FieldDescriptor::TYPE_MESSAGE ||
        result.field->label() != FieldDescriptor::LABEL_OPTIONAL) {
      result.message = NULL;
      result.field = NULL;
      return result;
    }

    result.message = reflection->MutableMessage(base, result.field);
    descriptor = result.message->GetDescriptor();
    reflection = result.message->GetReflection();
  }
}

bool AddText(const string& text, int node_type, Message* message) {
  FieldRef target = FindField(message, "text");

  if (target.field == NULL) {
    if (node_type == XML_NODE_SIGNIFICANTWHITESPACE) {
      return true;   // no need to record whitespace.
    } else {
      cerr << message->GetDescriptor()->full_name()
           << " has no \"text\" field.  Saw text: " << text << endl;
      return false;
    }
  } else {
    if (target.field->type() == FieldDescriptor::TYPE_STRING) {
      const Message::Reflection* reflection = target.message->GetReflection();
      string value = reflection->GetString(*target.message, target.field);
      value.append(text);
      reflection->SetString(target.message, target.field, value);
      return true;
    } else {
      cerr << target.field->full_name() << " is not a string." << endl;
      return false;
    }
  }
}

bool ReadAttributes(xmlTextReaderPtr reader, Message* message) {
  const Descriptor* descriptor = message->GetDescriptor();
  const Message::Reflection* reflection = message->GetReflection();

  if (xmlTextReaderHasAttributes(reader) > 0) {
    xmlTextReaderMoveToFirstAttribute(reader);
    do {
      string name = ToString(xmlTextReaderConstName(reader));
      string value = ToString(xmlTextReaderConstValue(reader));

      if (name.find_first_of(':') != string::npos) continue;

      const FieldDescriptor* field = descriptor->FindFieldByName(name);
      if (field == NULL) {
        cerr << descriptor->full_name() << " has no field \"" << name << "\"."
             << endl;
        return false;
      }
      if (!ParseSimpleField(message, reflection, field, value)) return false;
    } while (xmlTextReaderMoveToNextAttribute(reader) > 0);
    xmlTextReaderMoveToElement(reader);
  }

  return true;
}

bool SkipChildren(xmlTextReaderPtr reader) {
  if (xmlTextReaderIsEmptyElement(reader) > 0) return true;

  while (true) {
    int result = xmlTextReaderRead(reader);
    if (result <= 0) return false;
    switch (xmlTextReaderNodeType(reader)) {
      case XML_NODE_ELEMENT:
        if (!SkipChildren(reader)) return false;
        break;
      case XML_NODE_ENDELEMENT:
        return true;
      default:
        break;
    }
  }
}

bool ReadChildren(xmlTextReaderPtr reader, Message* message) {
  if (xmlTextReaderIsEmptyElement(reader) > 0) return true;

  while (true) {
    int result = xmlTextReaderRead(reader);
    if (result <= 0) return false;

    int node_type = xmlTextReaderNodeType(reader);
    switch (node_type) {
      case XML_NODE_ELEMENT: {
        string name = ToString(xmlTextReaderConstName(reader));

        FieldRef target = FindField(message, name);
        if (target.field == NULL) {
          cerr << message->GetDescriptor()->full_name() << " had no field \""
               << name << "\"." << endl;
          return false;
        }

        const Message::Reflection* target_reflection =
          target.message->GetReflection();

        if (target.field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
          Message* child;
          if (target.field->label() == FieldDescriptor::LABEL_REPEATED) {
            child = target_reflection->AddMessage(target.message, target.field);
          } else {
            child = target_reflection->MutableMessage(target.message, target.field);
          }
          if (!ReadAttributes(reader, child)) return false;
          if (!ReadChildren(reader, child)) return false;
        } else {
          if (xmlTextReaderHasAttributes(reader) > 0) {
            cerr << "Simple tag had attributes: " << target.field->full_name()
                 << endl;
            return false;
          }

          xmlChar* content = xmlTextReaderReadString(reader);
          string value = (content == NULL) ? "" : ToString(content);
          free(content);
          if (!ParseSimpleField(target.message, target_reflection, target.field, value)) {
            return false;
          }
          SkipChildren(reader);
        }
        break;
      }

      case XML_NODE_ENDELEMENT:
        return true;

      case XML_NODE_TEXT:
      case XML_NODE_CDATA:
      case XML_NODE_SIGNIFICANTWHITESPACE: {
        const xmlChar* value = xmlTextReaderConstValue(reader);
        if (value != NULL) {
          if (!AddText(ToString(value), node_type, message)) return false;
        }
        break;
      }

      case XML_NODE_WHITESPACE:
      case XML_NODE_COMMENT:
        // Ignore.
        break;

      case XML_NODE_NONE:
      case XML_NODE_ATTRIBUTE:
      case XML_NODE_ENTITYREFERENCE:
      case XML_NODE_ENTITY:
      case XML_NODE_PROCESSINGINSTRUCTION:
      case XML_NODE_DOCUMENT:
      case XML_NODE_DOCUMENTTYPE:
      case XML_NODE_DOCUMENTFRAGMENT:
      case XML_NODE_NOTATION:
      case XML_NODE_ENDENTITY:
      case XML_NODE_XMLDECLARATION:
        cerr << "Unexpected node type: " << NodeTypeName(node_type) << endl;
        return false;
      default:
        cerr << "Unknown node type: " << node_type << endl;
        return false;
    }
  }
}

bool ReadXmlRoot(xmlTextReaderPtr reader, Message* message) {
  int result = xmlTextReaderRead(reader);
  if (result <= 0) return result == 0;

  if (xmlTextReaderIsEmptyElement(reader) == 1) {
    cerr << "Document had no contents?" << endl;
    return false;
  }

  if (!ReadAttributes(reader, message)) return false;
  if (!ReadChildren(reader, message)) return false;

  return true;
}

// ===================================================================

enum ProtobufEncoding {
  ENCODING_TEXT,
  ENCODING_BINARY
};

class ErrorPrinter : public MultiFileErrorCollector {
 public:
  ErrorPrinter() {}
  ~ErrorPrinter() {}

  void AddError(const string& filename, int line, int column,
                const string& message) {
    if (line == -1) {
      cerr << filename << ": " << message << endl;
    } else {
      cerr << filename << ":" << (line + 1) << ":" << (column + 1)
           << ": " << message << endl;
    }
  }
};

bool Run(const string& proto_file_name, const string& type_name,
         ProtobufEncoding output_encoding) {
  DiskSourceTree source_tree;
  ErrorPrinter error_collector;
  source_tree.MapPath("", ".");
  Importer importer(&source_tree, &error_collector);

  const FileDescriptor* proto_file = importer.Import(proto_file_name);
  if (proto_file == NULL) {
    return false;
  }

  const Descriptor* type = proto_file->FindMessageTypeByName(type_name);
  if (type == NULL) {
    cerr << "No such type: " << type_name << endl;
    return false;
  }

  DynamicMessageFactory factory;
  Message* message = factory.GetPrototype(type)->New();

  // go!
  xmlTextReaderPtr xml_reader = xmlReaderForFd(0, "noname.xml", NULL, 0);
  if (xml_reader == NULL) {
    cerr << "Couldn't create XML reader." << endl;
    return false;
  }

  bool result = ReadXmlRoot(xml_reader, message);

  // Read the rest.
  while (xmlTextReaderRead(xml_reader) > 0) {}

  if (!message->IsInitialized()) {
    vector<string> errors;
    message->FindInitializationErrors(&errors);
    cerr << "Missing required fields:" << endl;
    for (size_t i = 0; i < errors.size(); i++) {
      cerr << "  " << errors[i] << endl;
    }
    result = false;
  }

  if (result) {
    io::FileOutputStream out(1);  // stdout
    switch (output_encoding) {
      case ENCODING_BINARY:
        message->SerializeToZeroCopyStream(&out);
        break;
      case ENCODING_TEXT:
        TextFormat::Print(*message, &out);
        break;
    }
  } else {
    cerr << "Failed to parse." << endl;
  }

  xmlFreeTextReader(xml_reader);
  delete message;
  return result;
}

// ===================================================================

const char* program_name_ = NULL;

void Usage() {
  cerr << "Usage:" << endl;
  cerr << "  " << program_name_
       << " --proto=PROTO --type=TYPE_NAME [--output=(text|binary)]"
       << endl;
  exit(1);
}

bool StartsWith(const char* str, const char* prefix) {
  return strncmp(str, prefix, strlen(prefix)) == 0;
}

bool ParseArg(const char* arg, const char* name, const char** value) {
  if (StartsWith(arg, name)) {
    if (value == NULL) {
      cerr << "Value not expected for flag: " << name << endl;
      Usage();
    }
    *value = arg + strlen(name);
    return true;
  }
  return false;
}

ProtobufEncoding ParseProtobufEncoding(const string& name) {
  if (name == "text") {
    return ENCODING_TEXT;
  } else if (name == "binary") {
    return ENCODING_BINARY;
  } else {
    cerr << "Unknown protobuf encoding: " << name << endl;
    Usage();
    return ENCODING_TEXT;  // Will never get here.
  }
}

int main(int argc, char* argv[]) {
  program_name_ = argv[0];
  const char* proto_file = NULL;
  const char* type_name = NULL;
  const char* output_encoding_name = NULL;
  ProtobufEncoding output_encoding = ENCODING_TEXT;

  LIBXML_TEST_VERSION

  --argc;
  ++argv;
  while (argc > 0) {
    if (ParseArg(argv[0], "--proto=", &proto_file) ||
        ParseArg(argv[0], "--type=", &type_name)) {
      // nothing to do here.
    } else if (ParseArg(argv[0], "--output=", &output_encoding_name)) {
      output_encoding = ParseProtobufEncoding(output_encoding_name);
    } else {
      cerr << "Unknown argument: " << argv[0] << endl;
      Usage();
    }
    --argc;
    ++argv;
  }

  if (proto_file == NULL) {
    cerr << "Missing flag: --proto" << endl;
    Usage();
  }
  if (type_name == NULL) {
    cerr << "Missing flag: --type" << endl;
    Usage();
  }

  if (Run(proto_file, type_name, output_encoding)) {
    return 0;
  } else {
    return 1;
  }
}
