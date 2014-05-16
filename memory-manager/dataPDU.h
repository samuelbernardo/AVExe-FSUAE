#ifndef __DATAPDU_H__
#define __DATAPDU_H__

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <openssl/sha.h>
#include "sysdeps.h"

class Contents {
	unsigned char contents_hash[SHA512_DIGEST_LENGTH];
	uaecptr addr;
	addrbank data;

    Contents(uaecptr addr, addrbank data, unsigned char contents_hash[]):
    	addr(addr),data(data),contents_hash(contents_hash){}
};

std::ostream& operator<<(std::ostream &out, const Contents &me){
    out << "addr: " << me.addr << " data: " << me.data;
    return out;
}

typedef Contents Container_Value_Type;
const Container_Value_Type defaultBad = Contents(-1, '!');
const size_t Container_Vector_Size = 4;

class Container{
    std::vector<Container_Value_Type> data;

    Container():data(Container_Vector_Size, defaultBad){}

    void serialize( std::ostream &stream ) const {
        for( size_t i = 0; i < Container_Vector_Size; ++i){
            stream.write( reinterpret_cast <const char*> ( &data[i] ), sizeof( Container_Value_Type ) );
        }
    }
    void deserialize( std::istream &stream ) {
        data.clear();
        std::vector<char> buffer(sizeof(Container_Value_Type));
        for( size_t i = 0; i < Container_Vector_Size; ++i){
            stream.read(&buffer[0], sizeof(Container_Value_Type));
            data.push_back(*reinterpret_cast<Container_Value_Type*>(&buffer[0]));
        }
    }
};

std::stringbuf& operator<<(std::stringbuf &stream, Container &me) {
    std::istream ibuffer(&stream);
    me.deserialize(ibuffer);
    return stream;
}

std::stringbuf& operator>>(std::stringbuf &stream, const Container &me) {
    std::ostream obuffer(&stream);
    me.serialize(obuffer);
    return stream;
}

std::ostream& operator<<(std::ostream &stream, const Container &me) {
    stream << "Container {" << std::endl;
    for( size_t i = 0; i < Container_Vector_Size; ++i){
        stream << i << ": " << me.data[i] << std::endl;
    }
    stream << "}" << std::endl << std::endl;
    return stream;
}

#endif
