function validateUsername(username, isRequired = true, minLength = 3, maxLength = 20) {
    const errors = [];
    
    if (!username && isRequired) {
        return {
            isValid: false,
            error: 'Vui lòng nhập tài khoản.'
        };
    }
    
    if (username && username.length < minLength) {
        return {
            isValid: false,
            error: `Tài khoản phải có ít nhất ${minLength} ký tự.`
        };
    }
    
    if (username && maxLength && username.length > maxLength) {
        return {
            isValid: false,
            error: `Tài khoản phải có từ ${minLength}-${maxLength} ký tự.`
        };
    }
    
    if (username && !/^[a-zA-Z0-9_]+$/.test(username)) {
        return {
            isValid: false,
            error: 'Tài khoản chỉ được chứa chữ cái, số và dấu gạch dưới.'
        };
    }
    
    return {
        isValid: true,
        error: null
    };
}

function validatePassword(password, isRequired = true, minLength = 6) {
    if (!password && isRequired) {
        return {
            isValid: false,
            error: 'Vui lòng nhập mật khẩu.'
        };
    }
    
    if (password && password.length < minLength) {
        return {
            isValid: false,
            error: `Mật khẩu phải có ít nhất ${minLength} ký tự.`
        };
    }
    
    return {
        isValid: true,
        error: null
    };
}

function validateConfirmPassword(password, confirmPassword) {
    if (!confirmPassword) {
        return {
            isValid: false,
            error: 'Vui lòng xác nhận mật khẩu.'
        };
    }
    
    if (password !== confirmPassword) {
        return {
            isValid: false,
            error: 'Mật khẩu xác nhận không khớp.'
        };
    }
    
    return {
        isValid: true,
        error: null
    };
}

export { validateUsername, validatePassword, validateConfirmPassword };