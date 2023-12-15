document.getElementById('get-code').addEventListener('click', function() {
    var email = document.getElementById('email').value;
    if(email) {
        // 发送请求到服务器获取验证码
        fetch('/api/get-code', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ email: email })
        })
        .then(response => response.json())
        .then(data => {
            if(data.status === 'error') {
                alert(data.message); // 显示错误信息
            } else {
                alert('验证码已发送，请检查您的邮箱');
                // 可以在这里添加倒计时逻辑
            }
        })
        .catch(error => console.error('Error:', error));
    } else {
        alert('请输入邮箱地址');
    }
});

document.querySelector('.login-form').addEventListener('submit', function(e) {
    e.preventDefault();
    var nickname = document.getElementById('nickname').value;
    var email = document.getElementById('email').value;
    var code = document.getElementById('verification-code').value;

    if(nickname && email && code) {
        // 发送注册信息到服务器
        fetch('/api/register', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ nickname: nickname, email: email, code: code })
        })
        .then(response => response.json())
        .then(data => {
            if(data.status === 'error') {
                alert(data.message); // 显示错误信息
            } else {
                alert('注册成功');
                // 可以在这里添加跳转到登录界面的逻辑
            }
        })
        .catch(error => console.error('Error:', error));
    } else {
        alert('请填写所有字段');
    }
});
