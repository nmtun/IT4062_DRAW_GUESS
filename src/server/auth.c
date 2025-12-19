#include "../include/auth.h"
#include "../include/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


// Kiểm tra tính hợp lệ của username
bool auth_validate_username(const char* username) {
    if (!username) {
        return false;
    }

    size_t len = strlen(username);
    
    // Check length: 3-32 characters
    if (len < 3 || len > 32) {
        fprintf(stderr, "Lỗi: Username phải có độ dài từ 3-32 ký tự\n");
        return false;
    }

    // Must start with letter or underscore
    if (!isalpha(username[0]) && username[0] != '_') {
        fprintf(stderr, "Lỗi: Username phải bắt đầu bằng chữ cái hoặc dấu gạch dưới\n");
        return false;
    }

    // Only alphanumeric and underscore
    for (size_t i = 0; i < len; i++) {
        if (!isalnum(username[i]) && username[i] != '_') {
            fprintf(stderr, "Lỗi: Username chỉ được chứa chữ cái, số và dấu gạch dưới\n");
            return false;
        }
    }

    return true;
}


// Kiểm tra tính hợp lệ của mật khẩu
bool auth_validate_password(const char* password) {
    if (!password) {
        return false;
    }

    size_t len = strlen(password);
    
    // Check length: 6-64 characters
    if (len < 6 || len > 64) {
        fprintf(stderr, "Lỗi: Mật khẩu phải có độ dài từ 6-64 ký tự\n");
        return false;
    }

    // Must contain at least one letter and one number
    bool has_letter = false;
    bool has_number = false;

    for (size_t i = 0; i < len; i++) {
        if (isalpha(password[i])) {
            has_letter = true;
        }
        if (isdigit(password[i])) {
            has_number = true;
        }
    }

    if (!has_letter || !has_number) {
        fprintf(stderr, "Lỗi: Mật khẩu phải chứa ít nhất một chữ cái và một số\n");
        return false;
    }

    return true;
}


// Hash mật khẩu sử dụng SHA256
int auth_hash_password(const char* password, char* hash_output) {
    if (!password || !hash_output) {
        fprintf(stderr, "Lỗi: Tham số không hợp lệ\n");
        return -1;
    }

    sha256_hex((const uint8_t*)password, strlen(password), hash_output);

    return 0;
}


//Xác thực mật khẩu bằng cách so sánh hash
bool auth_verify_password(const char* password, const char* hash) {
    if (!password || !hash) {
        return false;
    }

    char computed_hash[65];
    if (auth_hash_password(password, computed_hash) != 0) {
        return false;
    }

    return strcmp(computed_hash, hash) == 0;
}
