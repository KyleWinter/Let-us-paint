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
    var email = document.getElementById('email').value;
    var code = document.getElementById('verification-code').value;

    if(email && code) {
        // 发送登录信息到服务器
        fetch('/api/login', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ email: email, code: code })
        })
        .then(response => response.json())
        .then(data => {
            if(data.status === 'error') {
                alert(data.message); // 显示错误信息
            } else {
                alert('登录成功');
                window.location.href = '/new-page.html'; // 跳转到新的页面
            }
        })
        .catch(error => console.error('Error:', error));
    } else {
        alert('请填写所有字段');
    }
});
