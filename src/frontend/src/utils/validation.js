/**
 * Form validation utilities
 */

export const validators = {
    /**
     * Validate username (đồng bộ với server auth.c)
     */
    username: (value) => {
        if (!value || !value.trim()) {
            return 'Vui lòng nhập tài khoản';
        }
        
        const trimmed = value.trim();
        
        // Kiểm tra độ dài: 3-32 ký tự (đồng bộ với server)
        if (trimmed.length < 3) {
            return 'Tài khoản phải có ít nhất 3 ký tự';
        }
        
        if (trimmed.length > 32) {
            return 'Tài khoản không được quá 32 ký tự';
        }
        
        // Phải bắt đầu bằng chữ cái hoặc dấu gạch dưới (đồng bộ với server)
        if (!/^[a-zA-Z_]/.test(trimmed)) {
            return 'Tài khoản phải bắt đầu bằng chữ cái hoặc dấu gạch dưới';
        }
        
        // Chỉ được chứa chữ cái, số và dấu gạch dưới (đồng bộ với server)
        if (!/^[a-zA-Z0-9_]+$/.test(trimmed)) {
            return 'Tài khoản chỉ được chứa chữ cái, số và dấu gạch dưới';
        }
        
        return null;
    },

    /**
     * Validate password (đồng bộ với server auth.c)
     */
    password: (value) => {
        if (!value) {
            return 'Vui lòng nhập mật khẩu';
        }
        
        // Kiểm tra độ dài: 6-64 ký tự (đồng bộ với server)
        if (value.length < 6) {
            return 'Mật khẩu phải có ít nhất 6 ký tự';
        }
        
        if (value.length > 64) {
            return 'Mật khẩu không được quá 64 ký tự';
        }
        
        // Phải chứa ít nhất một chữ cái và một số (đồng bộ với server)
        const hasLetter = /[a-zA-Z]/.test(value);
        const hasNumber = /[0-9]/.test(value);
        
        if (!hasLetter || !hasNumber) {
            return 'Mật khẩu phải chứa ít nhất một chữ cái và một số';
        }
        
        return null;
    },

    /**
     * Validate confirm password
     */
    confirmPassword: (value, originalPassword) => {
        if (!value) {
            return 'Vui lòng xác nhận mật khẩu';
        }
        
        if (value !== originalPassword) {
            return 'Mật khẩu xác nhận không khớp';
        }
        
        return null;
    },
};

/**
 * Validate login form
 */
export const validateLoginForm = (formData) => {
    const errors = {};
    
    const usernameError = validators.username(formData.username);
    if (usernameError) errors.username = usernameError;
    
    const passwordError = validators.password(formData.password);
    if (passwordError) errors.password = passwordError;
    
    return {
        isValid: Object.keys(errors).length === 0,
        errors
    };
};

/**
 * Validate register form
 */
export const validateRegisterForm = (formData) => {
    const errors = {};
    
    const usernameError = validators.username(formData.username);
    if (usernameError) errors.username = usernameError;
    
    const passwordError = validators.password(formData.password);
    if (passwordError) errors.password = passwordError;
    
    const confirmPasswordError = validators.confirmPassword(formData.confirmPassword, formData.password);
    if (confirmPasswordError) errors.confirmPassword = confirmPasswordError;
    
    return {
        isValid: Object.keys(errors).length === 0,
        errors
    };
};

/**
 * Real-time field validation
 */
export const validateField = (fieldName, value, allFormData = {}) => {
    switch (fieldName) {
        case 'username':
            return validators.username(value);
        case 'password':
            return validators.password(value);
        case 'confirmPassword':
            return validators.confirmPassword(value, allFormData.password);
        default:
            return null;
    }
};

export default {
    validators,
    validateLoginForm,
    validateRegisterForm,
    validateField
};