// login.js - Login Page Logic
document.addEventListener('DOMContentLoaded', () => {
    const loginForm = document.getElementById('loginForm');
    const errorMessage = document.getElementById('errorMessage');
    
    loginForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        
        const username = document.getElementById('username').value;
        const password = document.getElementById('password').value;
        
        // Show loading
        const submitBtn = loginForm.querySelector('button[type="submit"]');
        const originalText = submitBtn.innerHTML;
        submitBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Logging in...';
        submitBtn.disabled = true;
        
        try {
            // Use proxy endpoint to avoid CORS issues
            const response = await fetch('/api', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    operation: 'user_login',
                    username: username,
                    password: password
                })
            });
            
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}`);
            }
            
            const result = await response.json();
            
            if (result.status === 'success' && result.data) {
                localStorage.setItem('session_id', result.data.session_id);
                localStorage.setItem('user_data', JSON.stringify({
                    driver_id: parseInt(result.data.driver_id) || 0,
                    name: result.data.name || username,
                    role: result.data.role || '0',
                    username: username
                }));
                
                window.location.href = 'index.html';
            } else {
                errorMessage.textContent = result.message || 'Invalid credentials';
                errorMessage.style.display = 'block';
            }
        } catch (error) {
            console.error('Login error:', error);
            let errorMsg = 'Connection failed. ';
            if (error.message.includes('Failed to fetch') || error.message.includes('NetworkError')) {
                errorMsg += 'Cannot connect to backend server. Please ensure:\n';
                errorMsg += '1. Backend server is running on port 8080\n';
                errorMsg += '2. No firewall is blocking the connection\n';
                errorMsg += '3. Check browser console for CORS errors';
            } else {
                errorMsg += error.message;
            }
            errorMessage.textContent = errorMsg;
            errorMessage.style.display = 'block';
            errorMessage.style.whiteSpace = 'pre-line';
        } finally {
            submitBtn.innerHTML = originalText;
            submitBtn.disabled = false;
        }
    });
    
});
