#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
白名单文件(.wl)解析工具 - GUI版本
用于读取和显示IEG白名单文件的内容
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from pathlib import Path
import sys

# 导入命令行版本的解析器
from wl_reader import WhitelistReader


class WhitelistReaderGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("白名单文件解析工具")
        self.root.geometry("1000x700")
        
        self.reader = None
        self.current_file = None
        
        self._create_widgets()
    
    def _create_widgets(self):
        """创建界面组件"""
        # 顶部工具栏
        toolbar = ttk.Frame(self.root, padding="5")
        toolbar.pack(side=tk.TOP, fill=tk.X)
        
        ttk.Button(toolbar, text="打开文件", command=self.open_file).pack(side=tk.LEFT, padx=5)
        ttk.Button(toolbar, text="导出文本", command=self.export_txt).pack(side=tk.LEFT, padx=5)
        ttk.Button(toolbar, text="刷新", command=self.refresh).pack(side=tk.LEFT, padx=5)
        
        # 文件信息区域
        info_frame = ttk.LabelFrame(self.root, text="文件信息", padding="10")
        info_frame.pack(side=tk.TOP, fill=tk.X, padx=10, pady=5)
        
        self.info_text = tk.Text(info_frame, height=4, wrap=tk.WORD)
        self.info_text.pack(fill=tk.X)
        
        # 搜索框
        search_frame = ttk.Frame(self.root, padding="5")
        search_frame.pack(side=tk.TOP, fill=tk.X, padx=10)
        
        ttk.Label(search_frame, text="搜索:").pack(side=tk.LEFT, padx=5)
        self.search_var = tk.StringVar()
        self.search_var.trace('w', self.on_search)
        search_entry = ttk.Entry(search_frame, textvariable=self.search_var, width=40)
        search_entry.pack(side=tk.LEFT, padx=5)
        
        # 白名单列表
        list_frame = ttk.LabelFrame(self.root, text="白名单列表", padding="10")
        list_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        # 创建Treeview
        columns = ("序号", "文件名", "文件路径", "操作", "系统文件", "Hash类型", "Hash值")
        self.tree = ttk.Treeview(list_frame, columns=columns, show='headings', height=20)
        
        # 设置列宽
        self.tree.column("序号", width=50, anchor=tk.CENTER)
        self.tree.column("文件名", width=150)
        self.tree.column("文件路径", width=300)
        self.tree.column("操作", width=60, anchor=tk.CENTER)
        self.tree.column("系统文件", width=80, anchor=tk.CENTER)
        self.tree.column("Hash类型", width=80, anchor=tk.CENTER)
        self.tree.column("Hash值", width=250)
        
        # 设置列标题
        for col in columns:
            self.tree.heading(col, text=col, command=lambda c=col: self.sort_column(c))
        
        # 滚动条
        vsb = ttk.Scrollbar(list_frame, orient="vertical", command=self.tree.yview)
        hsb = ttk.Scrollbar(list_frame, orient="horizontal", command=self.tree.xview)
        self.tree.configure(yscrollcommand=vsb.set, xscrollcommand=hsb.set)
        
        self.tree.grid(row=0, column=0, sticky='nsew')
        vsb.grid(row=0, column=1, sticky='ns')
        hsb.grid(row=1, column=0, sticky='ew')
        
        list_frame.grid_rowconfigure(0, weight=1)
        list_frame.grid_columnconfigure(0, weight=1)
        
        # 状态栏
        self.status_var = tk.StringVar()
        self.status_var.set("就绪")
        status_bar = ttk.Label(self.root, textvariable=self.status_var, relief=tk.SUNKEN, anchor=tk.W)
        status_bar.pack(side=tk.BOTTOM, fill=tk.X)
    
    def open_file(self):
        """打开文件对话框"""
        file_path = filedialog.askopenfilename(
            title="选择白名单文件",
            filetypes=[("白名单文件", "*.wl"), ("所有文件", "*.*")]
        )
        
        if file_path:
            self.load_file(file_path)
    
    def load_file(self, file_path):
        """加载白名单文件"""
        try:
            self.status_var.set(f"正在加载: {file_path}")
            self.root.update()
            
            self.reader = WhitelistReader(file_path)
            if not self.reader.read():
                messagebox.showerror("错误", "无法读取文件")
                return
            
            self.current_file = file_path
            self.display_info()
            self.display_entries()
            
            self.status_var.set(f"已加载: {Path(file_path).name} - {len(self.reader.entries)} 条记录")
        
        except Exception as e:
            messagebox.showerror("错误", f"加载文件时出错:\n{e}")
            self.status_var.set("加载失败")
    
    def display_info(self):
        """显示文件信息"""
        if not self.reader:
            return
        
        self.info_text.delete(1.0, tk.END)
        info = (
            f"文件名: {Path(self.current_file).name}\n"
            f"文件路径: {self.current_file}\n"
            f"文件版本: 0x{self.reader.version:04X}\n"
            f"白名单数量: {len(self.reader.entries)} 条"
        )
        self.info_text.insert(1.0, info)
    
    def display_entries(self, filter_text=""):
        """显示白名单条目"""
        # 清空现有数据
        for item in self.tree.get_children():
            self.tree.delete(item)
        
        if not self.reader:
            return
        
        # 添加数据
        for i, entry in enumerate(self.reader.entries, 1):
            # 过滤
            if filter_text:
                if filter_text.lower() not in entry.full_path.lower() and \
                   filter_text.lower() not in entry.file_hash.lower():
                    continue
            
            # 提取文件名
            file_name = Path(entry.full_path).name if entry.full_path else ""
            
            # 操作类型
            action = "添加" if entry.add_or_del == 1 else "删除" if entry.add_or_del == 2 else f"未知({entry.add_or_del})"
            
            # 系统文件
            sys_file = "是" if entry.is_system_file else "否"
            
            # Hash类型
            hash_type = "SHA1" if entry.hash_type == 1 else "MD5" if entry.hash_type == 2 else f"未知({entry.hash_type})"
            
            self.tree.insert("", tk.END, values=(
                i,
                file_name,
                entry.full_path,
                action,
                sys_file,
                hash_type,
                entry.file_hash
            ))
    
    def on_search(self, *args):
        """搜索事件"""
        if self.reader:
            filter_text = self.search_var.get()
            self.display_entries(filter_text)
    
    def sort_column(self, col):
        """排序列"""
        # TODO: 实现排序功能
        pass
    
    def export_txt(self):
        """导出到文本文件"""
        if not self.reader:
            messagebox.showwarning("警告", "请先打开一个白名单文件")
            return
        
        file_path = filedialog.asksaveasfilename(
            title="导出文本文件",
            defaultextension=".txt",
            filetypes=[("文本文件", "*.txt"), ("所有文件", "*.*")]
        )
        
        if file_path:
            if self.reader.export_to_txt(file_path):
                messagebox.showinfo("成功", f"已导出到:\n{file_path}")
            else:
                messagebox.showerror("错误", "导出失败")
    
    def refresh(self):
        """刷新"""
        if self.current_file:
            self.load_file(self.current_file)


def main():
    root = tk.Tk()
    app = WhitelistReaderGUI(root)
    
    # 如果命令行提供了文件路径，直接加载
    if len(sys.argv) > 1:
        app.load_file(sys.argv[1])
    
    root.mainloop()


if __name__ == '__main__':
    main()
