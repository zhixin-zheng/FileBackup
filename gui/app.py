import sys
import os
import time
from datetime import datetime

# --- 环境路径配置 ---
current_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(current_dir)
build_dir = os.path.join(project_root, "build")

# 将 build 目录添加到 Python 搜索路径中
if os.path.exists(build_dir):
    if build_dir not in sys.path:
        sys.path.insert(0, build_dir) # 插到最前面，优先搜索
        print(f"[Init] Added build directory to path: {build_dir}")
else:
    print(f"[Init] Warning: Build directory not found at: {build_dir}")

try:
    import backup_core_py as core
    print(f"[Init] Successfully loaded C++ Core: {core}")
except ImportError:
    print("错误: 无法找到 'backup_core_py' 模块。请先编译 C++ 核心。")
    print("提示: 将 build 目录下的 .so/.pyd 文件复制到当前目录。")

from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QLabel, QLineEdit, QPushButton, 
                             QTabWidget, QFileDialog, QComboBox, QCheckBox, 
                             QGroupBox, QProgressBar, QMessageBox, QDateEdit,
                             QSpinBox, QTextEdit)
from PyQt6.QtCore import Qt, QThread, pyqtSignal, QDate, QDir

# --- 样式表 (美化) ---
STYLESHEET = """
QMainWindow {
    background-color: #f0f2f5;
}
QTabWidget::pane {
    border: 1px solid #dcdcdc;
    background: white;
    border-radius: 5px;
}
QTabBar::tab {
    background: #e1e4e8;
    padding: 10px 20px;
    border-top-left-radius: 5px;
    border-top-right-radius: 5px;
    margin-right: 2px;
}
QTabBar::tab:selected {
    background: white;
    font-weight: bold;
    color: #1a73e8;
}
QGroupBox {
    font-weight: bold;
    border: 1px solid #dcdcdc;
    border-radius: 5px;
    margin-top: 10px;
    padding-top: 15px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 3px 0 3px;
}
QPushButton {
    background-color: #1a73e8;
    color: white;
    border-radius: 4px;
    padding: 6px 15px;
    font-weight: bold;
}
QPushButton:hover {
    background-color: #1557b0;
}
QPushButton:disabled {
    background-color: #a0c2f0;
}
QLineEdit {
    padding: 5px;
    border: 1px solid #ccc;
    border-radius: 3px;
}
QProgressBar {
    border: 1px solid #ccc;
    border-radius: 3px;
    text-align: center;
}
QProgressBar::chunk {
    background-color: #34a853;
}
"""

# --- 后台工作线程 ---
class WorkerThread(QThread):
    finished = pyqtSignal(bool, str) # success, message

    def __init__(self, task_type, sys_obj, *args):
        super().__init__()
        self.task_type = task_type
        self.sys = sys_obj
        self.args = args

    def run(self):
        try:
            if self.task_type == "backup":
                src, dst = self.args
                success = self.sys.backup(src, dst)
                msg = "备份成功！" if success else "备份失败，请检查日志。"
            elif self.task_type == "restore":
                src, dst = self.args
                success = self.sys.restore(src, dst)
                msg = "还原成功！" if success else "还原失败（密码错误或文件损坏）。"
            elif self.task_type == "verify":
                src = self.args[0]
                success = self.sys.verify(src)
                msg = "验证通过：文件完整。" if success else "验证失败：文件已损坏或密码错误。"
            
            self.finished.emit(success, msg)
        except Exception as e:
            self.finished.emit(False, str(e))

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("FileBackup")
        self.resize(700, 650)
        self.setStyleSheet(STYLESHEET)

        self.backup_system = core.BackupSystem()
        self.scheduler = core.BackupScheduler() 
        self.scheduler.start() 
        
        # 主布局
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)

        # 选项卡
        self.tabs = QTabWidget()
        main_layout.addWidget(self.tabs)

        self.init_backup_tab()
        self.init_restore_tab()
        self.init_schedule_tab() 

        # 底部状态
        self.status_label = QLabel("Ready")
        self.status_label.setStyleSheet("color: #666; margin-top: 5px;")
        main_layout.addWidget(self.status_label)

    def closeEvent(self, event):
        self.scheduler.stop()
        event.accept()

    def init_backup_tab(self):
        tab = QWidget()
        layout = QVBoxLayout(tab)
        
        # 1. 源与目标
        grp_io = QGroupBox("输入/输出设置")
        layout_io = QVBoxLayout()
        
        # 源路径（可选目录或单个文件）
        h1 = QHBoxLayout()
        self.src_dir_edit = QLineEdit()
        self.src_dir_edit.setPlaceholderText("选择要备份的源目录或单个文件...")
        btn_src = QPushButton("浏览...")
        btn_src.clicked.connect(lambda: self.browse_path(self.src_dir_edit))
        h1.addWidget(QLabel("源路径:"))
        h1.addWidget(self.src_dir_edit)
        h1.addWidget(btn_src)
        layout_io.addLayout(h1)

        # 目标目录（或目标文件）
        h2 = QHBoxLayout()
        self.dst_file_edit = QLineEdit()
        self.dst_file_edit.setPlaceholderText("选择存放目录 (留空则默认在源目录同级生成 .bin)")
        btn_dst = QPushButton("浏览...")
        btn_dst.clicked.connect(lambda: self.browse_dir(self.dst_file_edit))
        h2.addWidget(QLabel("存放目录:"))
        h2.addWidget(self.dst_file_edit)
        h2.addWidget(btn_dst)
        layout_io.addLayout(h2)
        
        grp_io.setLayout(layout_io)
        layout.addWidget(grp_io)

        # 2. 压缩与加密
        grp_opt = QGroupBox("安全与压缩")
        layout_opt = QHBoxLayout()

        self.combo_algo = QComboBox()
        self.combo_algo.addItems(["Huffman (更紧凑)", "LZSS (更快)", "Joined (混合)"])
        self.combo_algo.setCurrentIndex(1) 
        self.pwd_edit = QLineEdit()
        self.pwd_edit.setPlaceholderText("设置密码 (留空则不加密)")
        self.pwd_edit.setEchoMode(QLineEdit.EchoMode.Password)

        layout_opt.addWidget(QLabel("算法:"))
        layout_opt.addWidget(self.combo_algo)
        layout_opt.addWidget(QLabel("   密码:"))
        layout_opt.addWidget(self.pwd_edit)
        grp_opt.setLayout(layout_opt)
        layout.addWidget(grp_opt)

        # 3. 高级过滤
        grp_filter = QGroupBox("高级过滤 (可选)")
        grp_filter.setCheckable(True)
        grp_filter.setChecked(False)
        self.grp_filter = grp_filter 
        layout_filter = QVBoxLayout()

        # 后缀名
        h_ext = QHBoxLayout()
        self.ext_edit = QLineEdit()
        self.ext_edit.setPlaceholderText("例如: .txt .cpp .jpg (空格分隔)")
        h_ext.addWidget(QLabel("文件后缀:"))
        h_ext.addWidget(self.ext_edit)
        layout_filter.addLayout(h_ext)

        # 文件名正则
        h_regex = QHBoxLayout()
        self.regex_edit = QLineEdit()
        self.regex_edit.setPlaceholderText("例如: ^report_.* (正则匹配)")
        h_regex.addWidget(QLabel("文件名正则:"))
        h_regex.addWidget(self.regex_edit)
        layout_filter.addLayout(h_regex)

        # 大小限制
        h_size = QHBoxLayout()
        self.min_size_edit = QLineEdit("0")
        self.max_size_edit = QLineEdit("0")
        h_size.addWidget(QLabel("最小字节:"))
        h_size.addWidget(self.min_size_edit)
        h_size.addWidget(QLabel("最大字节 (0=无限):"))
        h_size.addWidget(self.max_size_edit)
        layout_filter.addLayout(h_size)

        grp_filter.setLayout(layout_filter)
        layout.addWidget(grp_filter)

        # 4. 操作区
        self.progress_bar = QProgressBar()
        self.progress_bar.setValue(0)
        self.btn_start_backup = QPushButton("开始备份")
        self.btn_start_backup.setFixedHeight(40)
        self.btn_start_backup.setStyleSheet("background-color: #34a853; font-size: 14px;")
        self.btn_start_backup.clicked.connect(self.run_backup)

        layout.addStretch()
        layout.addWidget(self.progress_bar)
        layout.addWidget(self.btn_start_backup)

        self.tabs.addTab(tab, "备份 (Backup)")

    def init_restore_tab(self):
        tab = QWidget()
        layout = QVBoxLayout(tab)

        grp_io = QGroupBox("还原设置")
        layout_io = QVBoxLayout()

        # 备份文件选择
        h1 = QHBoxLayout()
        self.res_src_edit = QLineEdit()
        btn_src = QPushButton("选择文件...")
        btn_src.clicked.connect(lambda: self.browse_file(self.res_src_edit))
        h1.addWidget(QLabel("备份文件:"))
        h1.addWidget(self.res_src_edit)
        h1.addWidget(btn_src)
        layout_io.addLayout(h1)

        # 目标目录
        h2 = QHBoxLayout()
        self.res_dst_edit = QLineEdit()
        btn_dst = QPushButton("选择目录...")
        btn_dst.clicked.connect(lambda: self.browse_dir(self.res_dst_edit))
        h2.addWidget(QLabel("还原位置:"))
        h2.addWidget(self.res_dst_edit)
        h2.addWidget(btn_dst)
        layout_io.addLayout(h2)

        # 解密密码
        h3 = QHBoxLayout()
        self.res_pwd_edit = QLineEdit()
        self.res_pwd_edit.setPlaceholderText("如果已加密，请输入密码")
        self.res_pwd_edit.setEchoMode(QLineEdit.EchoMode.Password)
        h3.addWidget(QLabel("解密密码:"))
        h3.addWidget(self.res_pwd_edit)
        layout_io.addLayout(h3)

        grp_io.setLayout(layout_io)
        layout.addWidget(grp_io)

        # 操作按钮
        h_btns = QHBoxLayout()
        self.btn_verify = QPushButton("仅验证完整性")
        self.btn_verify.clicked.connect(self.run_verify)
        
        self.btn_restore = QPushButton("开始还原")
        self.btn_restore.setFixedHeight(40)
        self.btn_restore.clicked.connect(self.run_restore)

        h_btns.addWidget(self.btn_verify)
        h_btns.addWidget(self.btn_restore)

        layout.addStretch()
        layout.addLayout(h_btns)
        
        self.tabs.addTab(tab, "还原 (Restore)")

    # --- 计划任务页面 ---
    def init_schedule_tab(self):
        tab = QWidget()
        layout = QVBoxLayout(tab)

        grp_cfg = QGroupBox("自动化任务配置")
        l_cfg = QVBoxLayout()

        # 1. 监控源目录
        h_src = QHBoxLayout()
        self.sch_src_edit = QLineEdit()
        self.sch_src_edit.setPlaceholderText("选择需要持续监控或定期备份的目录")
        btn_src = QPushButton("监控目录...")
        btn_src.clicked.connect(lambda: self.browse_dir(self.sch_src_edit))
        h_src.addWidget(QLabel("源目录:"))
        h_src.addWidget(self.sch_src_edit)
        h_src.addWidget(btn_src)
        l_cfg.addLayout(h_src)

        # 2. 存放目录
        h_dst = QHBoxLayout()
        self.sch_dst_edit = QLineEdit()
        self.sch_dst_edit.setPlaceholderText("选择存放自动备份文件的文件夹")
        btn_dst = QPushButton("存放目录...")
        btn_dst.clicked.connect(lambda: self.browse_dir(self.sch_dst_edit))
        h_dst.addWidget(QLabel("存放目录:"))
        h_dst.addWidget(self.sch_dst_edit)
        h_dst.addWidget(btn_dst)
        l_cfg.addLayout(h_dst)

        # 3. 安全与压缩
        h_sec = QHBoxLayout()
        self.sch_combo_algo = QComboBox()
        self.sch_combo_algo.addItems(["Huffman", "LZSS", "Joined"])
        self.sch_combo_algo.setCurrentIndex(1)
        
        self.sch_pwd_edit = QLineEdit()
        self.sch_pwd_edit.setPlaceholderText("自动备份加密密码")
        self.sch_pwd_edit.setEchoMode(QLineEdit.EchoMode.Password)
        
        h_sec.addWidget(QLabel("算法:"))
        h_sec.addWidget(self.sch_combo_algo)
        h_sec.addWidget(QLabel("密码:"))
        h_sec.addWidget(self.sch_pwd_edit)
        l_cfg.addLayout(h_sec)

        # 4. 模式选择
        h_mode = QHBoxLayout()
        self.combo_mode = QComboBox()
        self.combo_mode.addItems(["定时备份 (Interval)", "实时监控 (Real-time)"])
        
        self.spin_interval = QSpinBox()
        self.spin_interval.setRange(5, 86400) 
        self.spin_interval.setValue(60)
        self.spin_interval.setSuffix(" 秒")
        
        h_mode.addWidget(QLabel("模式:"))
        h_mode.addWidget(self.combo_mode)
        h_mode.addWidget(QLabel("间隔:"))
        h_mode.addWidget(self.spin_interval)
        l_cfg.addLayout(h_mode)

        # 5. 淘汰策略
        h_prune = QHBoxLayout()
        self.sch_prefix = QLineEdit("auto_backup")
        self.sch_prefix.setPlaceholderText("文件名前缀")
        
        self.spin_keep = QSpinBox()
        self.spin_keep.setRange(1, 100)
        self.spin_keep.setValue(5)
        
        h_prune.addWidget(QLabel("文件前缀:"))
        h_prune.addWidget(self.sch_prefix)
        h_prune.addWidget(QLabel("保留副本数:"))
        h_prune.addWidget(self.spin_keep)
        l_cfg.addLayout(h_prune)

        # 6. 添加按钮
        self.btn_add_task = QPushButton("添加并启动任务")
        self.btn_add_task.clicked.connect(self.add_schedule_task)
        l_cfg.addWidget(self.btn_add_task)

        grp_cfg.setLayout(l_cfg)
        layout.addWidget(grp_cfg)

        # 7. 任务日志
        self.log_area = QTextEdit()
        self.log_area.setReadOnly(True)
        self.log_area.append("调度器已就绪。")
        layout.addWidget(QLabel("任务运行日志:"))
        layout.addWidget(self.log_area)

        self.tabs.addTab(tab, "计划任务 (Scheduler)")

    # --- 逻辑处理 ---

    def browse_dir(self, line_edit):
        d = QFileDialog.getExistingDirectory(self, "选择目录")
        if d: line_edit.setText(d)

    def save_file(self, line_edit):
        f, _ = QFileDialog.getSaveFileName(self, "保存备份", "", "Backup Files (*.bin);;All Files (*)")
        if f: line_edit.setText(f)

    def browse_file(self, line_edit):
        f, _ = QFileDialog.getOpenFileName(self, "打开备份", "", "Backup Files (*.bin);;All Files (*)")
        if f: line_edit.setText(f)

    def browse_path(self, line_edit):
        """
        统一选择器：既可选目录也可选文件（通过非原生对话框允许目录选择）
        """
        dlg = QFileDialog(self, "选择源路径")
        dlg.setFileMode(QFileDialog.FileMode.Directory)
        dlg.setOption(QFileDialog.Option.DontUseNativeDialog, True)
        dlg.setOption(QFileDialog.Option.ShowDirsOnly, False)
        dlg.setOption(QFileDialog.Option.ReadOnly, True)
        dlg.setFilter(QDir.Filter.AllEntries | QDir.Filter.NoDotAndDotDot | QDir.Filter.AllDirs | QDir.Filter.Files)
        if dlg.exec():
            files = dlg.selectedFiles()
            if files:
                line_edit.setText(files[0])

    def lock_ui(self, locked):
        self.btn_start_backup.setEnabled(not locked)
        self.btn_restore.setEnabled(not locked)
        self.btn_verify.setEnabled(not locked)
        self.progress_bar.setRange(0, 0 if locked else 100)

    def run_backup(self):
        src = self.src_dir_edit.text()
        dst = self.dst_file_edit.text()
        pwd = self.pwd_edit.text()
        
        if not src or not dst:
            QMessageBox.warning(self, "提示", "请填写源目录和目标文件路径")
            return

        algo_idx = self.combo_algo.currentIndex() 
        self.backup_system.setCompressionAlgorithm(algo_idx)
        self.backup_system.setPassword(pwd)

        if self.grp_filter.isChecked():
            opts = core.FilterOptions()
            opts.enabled = True
            opts.nameRegex = self.regex_edit.text()
            
            ext_str = self.ext_edit.text().strip()
            if ext_str:
                opts.suffixes = [e if e.startswith('.') else f".{e}" for e in ext_str.split()]
            
            try:
                opts.minSize = int(self.min_size_edit.text())
                opts.maxSize = int(self.max_size_edit.text())
            except ValueError:
                pass
            
            self.backup_system.setFilter(opts)
        else:
            opts = core.FilterOptions()
            opts.enabled = False
            self.backup_system.setFilter(opts)

        self.start_worker("backup", src, dst)

    def run_restore(self):
        src = self.res_src_edit.text()
        dst = self.res_dst_edit.text()
        pwd = self.res_pwd_edit.text()
        
        if not src or not dst:
            QMessageBox.warning(self, "提示", "请填写备份文件路径和还原目录")
            return

        self.backup_system.setPassword(pwd)
        self.start_worker("restore", src, dst)

    def run_verify(self):
        src = self.res_src_edit.text()
        pwd = self.res_pwd_edit.text()
        if not src: return
        
        self.backup_system.setPassword(pwd)
        self.start_worker("verify", src)

    def add_schedule_task(self):
        src = self.sch_src_edit.text()
        dst = self.sch_dst_edit.text()
        prefix = self.sch_prefix.text()
        keep = self.spin_keep.value()
        pwd = self.sch_pwd_edit.text()
        algo = self.sch_combo_algo.currentIndex()

        if not src or not dst:
            QMessageBox.warning(self, "错误", "源目录和存放目录不能为空")
            return

        is_realtime = (self.combo_mode.currentIndex() == 1)
        task_id = -1
        
        if is_realtime:
            task_id = self.scheduler.addRealtimeTask(src, dst, prefix, keep)
            self.log_area.append(f"[{datetime.now().strftime('%H:%M:%S')}] 添加实时监控任务 ID: {task_id}")
            self.log_area.append(f"   源: {src}\n   目标: {dst}")
        else:
            interval = self.spin_interval.value()
            task_id = self.scheduler.addScheduledTask(src, dst, prefix, interval, keep)
            self.log_area.append(f"[{datetime.now().strftime('%H:%M:%S')}] 添加定时任务 ID: {task_id}")
            self.log_area.append(f"   每 {interval} 秒执行一次")

        self.scheduler.setTaskPassword(task_id, pwd)
        self.scheduler.setTaskCompressionAlgorithm(task_id, algo)

    def start_worker(self, task, *args):
        self.lock_ui(True)
        self.status_label.setText(f"Status: Running {task}...")
        
        self.worker = WorkerThread(task, self.backup_system, *args)
        self.worker.finished.connect(self.on_worker_finished)
        self.worker.start()

    def on_worker_finished(self, success, msg):
        self.lock_ui(False)
        self.progress_bar.setRange(0, 100)
        self.progress_bar.setValue(100 if success else 0)
        self.status_label.setText("Status: Idle")
        
        icon = QMessageBox.Icon.Information if success else QMessageBox.Icon.Critical
        QMessageBox(icon, "结果", msg, QMessageBox.StandardButton.Ok, self).exec()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())