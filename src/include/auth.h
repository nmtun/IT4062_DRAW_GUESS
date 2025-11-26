#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>

/**
 * Validate username format
 * @param username Username to validate
 * @return true if valid, false otherwise
 * 
 * Rules:
 * - Length: 3-32 characters
 * - Only alphanumeric and underscore
 * - Must start with letter or underscore
 */
bool auth_validate_username(const char* username);

/**
 * Validate password format
 * @param password Password to validate
 * @return true if valid, false otherwise
 * 
 * Rules:
 * - Length: 6-64 characters
 * - Must contain at least one letter and one number
 */
bool auth_validate_password(const char* password);

/**
 * Hash password using SHA256
 * @param password Plain text password
 * @param hash_output Buffer to store hash (must be at least 65 bytes for hex string)
 * @return 0 if success, -1 if error
 */
int auth_hash_password(const char* password, char* hash_output);

/**
 * Verify password against hash
 * @param password Plain text password
 * @param hash Stored hash to compare
 * @return true if match, false otherwise
 */
bool auth_verify_password(const char* password, const char* hash);

#endif // AUTH_H
