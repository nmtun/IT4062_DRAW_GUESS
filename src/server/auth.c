#include "../include/auth.h"
#include "../include/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


// Kiem tra tinh hop le cua username
bool auth_validate_username(const char* username) {
    if (!username) {
        return false;
    }

    size_t len = strlen(username);
    
    // Check length: 3-32 characters
    if (len < 3 || len > 32) {
        fprintf(stderr, "Loi: Username phai co do dai tu 3-32 ky tu\n");
        return false;
    }

    // Must start with letter or underscore
    if (!isalpha(username[0]) && username[0] != '_') {
        fprintf(stderr, "Loi: Username phai bat dau bang chu cai hoac dau gach duoi\n");
        return false;
    }

    // Only alphanumeric and underscore
    for (size_t i = 0; i < len; i++) {
        if (!isalnum(username[i]) && username[i] != '_') {
            fprintf(stderr, "Loi: Username chi duoc chua chu cai, so va dau gach duoi\n");
            return false;
        }
    }

    return true;
}


// Kiem tra tinh hop le cua mat khau
bool auth_validate_password(const char* password) {
    if (!password) {
        return false;
    }

    size_t len = strlen(password);
    
    // Check length: 6-64 characters
    if (len < 6 || len > 64) {
        fprintf(stderr, "Loi: Mat khau phai co do dai tu 6-64 ky tu\n");
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
        fprintf(stderr, "Loi: Mat khau phai chua it nhat mot chu cai va mot so\n");
        return false;
    }

    return true;
}


// Hash mat khau su dung SHA256
int auth_hash_password(const char* password, char* hash_output) {
    if (!password || !hash_output) {
        fprintf(stderr, "Loi: Tham so khong hop le\n");
        return -1;
    }

    sha256_hex((const uint8_t*)password, strlen(password), hash_output);

    return 0;
}


// Xac thuc mat khau bang cach so sanh hash
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
