// Native AppKit test converter. The codec is statically linked; no child process.
#import <AppKit/AppKit.h>
#include "encoder_api.hpp"
#include "decoder_api.hpp"
#include "codec_profiles.hpp"
#include "gui_i18n.hpp"
#include "gui_jobs.hpp"
#include "platform_io.hpp"
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

namespace ui=atracdenc_aal_gui;
namespace fs=std::filesystem;
static NSString* ns(const std::wstring& text) {
    const auto value=independent_aal::utf8(text);
    return [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding] ?: @"";
}
static NSString* text(const wchar_t* value) { return ns(ui::tr(value)); }
static std::wstring aal_wide(NSString* value) {
    static_assert(sizeof(wchar_t)==4, "macOS wchar_t must be UTF-32");
    NSData* data=[value dataUsingEncoding:NSUTF32LittleEndianStringEncoding];
    std::wstring out(data.length/4,L'\0');
    if (data.length) std::memcpy(out.data(),data.bytes,data.length);
    return out;
}
static NSString* filename(const fs::path& path) {
    const auto value=path.filename().u8string();
    return [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding] ?: @"?";
}
static fs::path file_path(NSURL* url) { return fs::path(url.fileSystemRepresentation); }

@class AALController;
@interface AALDropView : NSView
@property(nonatomic,weak) AALController* controller;
@end

enum class State { Pending, Working, Done, Failed, Cancelled };
struct Row { fs::path input,output; State state=State::Pending; int percent=0; };

@interface AALController : NSObject<NSApplicationDelegate,NSWindowDelegate,NSTableViewDataSource,NSTableViewDelegate> {
    NSWindow* _window;
    NSTableView* _table;
    NSTextView* _log;
    NSTextField* _output;
    NSTextField* _phase;
    NSTextField* _summary;
    NSProgressIndicator* _progress;
    NSPopUpButton* _profile;
    NSButton* _add;
    NSButton* _remove;
    NSButton* _clear;
    NSButton* _browse;
    NSButton* _start;
    NSButton* _cancel;
    NSButton* _selfTest;
    BOOL _busy,_closing,_terminating;
    std::atomic_bool _cancelled;
    std::thread _worker;
    std::vector<Row> _rows;
    NSArray<NSURL*>* _startupURLs;
}
- (instancetype)initWithURLs:(NSArray<NSURL*>*)urls;
- (void)addURLs:(NSArray<NSURL*>*)urls;
- (void)log:(NSString*)message;
- (void)updateControls;
- (void)beginWork:(BOOL)testOnly;
@end

@implementation AALDropView
- (BOOL)isFlipped { return YES; }
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    return [sender.draggingPasteboard canReadObjectForClasses:@[NSURL.class]
        options:@{NSPasteboardURLReadingFileURLsOnlyKey:@YES}] ? NSDragOperationCopy : NSDragOperationNone;
}
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    NSArray<NSURL*>* urls=[sender.draggingPasteboard readObjectsForClasses:@[NSURL.class]
        options:@{NSPasteboardURLReadingFileURLsOnlyKey:@YES}];
    if (!urls.count) return NO;
    [self.controller addURLs:urls];
    return YES;
}
@end

static NSTextField* label(NSString* value, CGFloat size=13) {
    NSTextField* field=[NSTextField wrappingLabelWithString:value];
    field.font=[NSFont systemFontOfSize:size];
    return field;
}
static NSButton* button(NSString* title,id target,SEL action) {
    NSButton* control=[NSButton buttonWithTitle:title target:target action:action];
    control.bezelStyle=NSBezelStyleRounded;
    return control;
}
static NSStackView* horizontal(NSArray<NSView*>* views) {
    NSStackView* row=[NSStackView stackViewWithViews:views];
    row.orientation=NSUserInterfaceLayoutOrientationHorizontal;
    row.alignment=NSLayoutAttributeCenterY;
    row.spacing=8;
    return row;
}

@implementation AALController
- (instancetype)initWithURLs:(NSArray<NSURL*>*)urls {
    if ((self=[super init])) { _startupURLs=[urls copy]; _cancelled.store(false); }
    return self;
}
- (void)dealloc {
    _cancelled.store(true);
    if (_worker.joinable()) _worker.join();
}
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    NSMenu* menu=[NSMenu new];
    NSMenuItem* appItem=[NSMenuItem new];
    NSMenu* appMenu=[NSMenu new];
    [appMenu addItemWithTitle:text(L"退出 AtracDEnc-AAL") action:@selector(terminate:) keyEquivalent:@"q"];
    appItem.submenu=appMenu; [menu addItem:appItem];
    NSMenuItem* editItem=[NSMenuItem new]; editItem.title=text(L"编辑");
    NSMenu* editMenu=[NSMenu new];
    [editMenu addItemWithTitle:text(L"剪切") action:@selector(cut:) keyEquivalent:@"x"];
    [editMenu addItemWithTitle:text(L"复制") action:@selector(copy:) keyEquivalent:@"c"];
    [editMenu addItemWithTitle:text(L"粘贴") action:@selector(paste:) keyEquivalent:@"v"];
    [editMenu addItemWithTitle:text(L"全选") action:@selector(selectAll:) keyEquivalent:@"a"];
    editItem.submenu=editMenu; [menu addItem:editItem]; NSApp.mainMenu=menu;

    _window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,920,750)
        styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable
        backing:NSBackingStoreBuffered defer:NO];
    _window.title=text(L"AtracDEnc-AAL · 简易测试文件转换器 0.5.0");
    _window.contentMinSize=NSMakeSize(840,720);
    _window.delegate=self;
    _window.releasedWhenClosed=NO;
    AALDropView* content=[[AALDropView alloc] initWithFrame:_window.contentView.bounds];
    content.controller=self;
    [content registerForDraggedTypes:@[NSPasteboardTypeFileURL]];
    _window.contentView=content;
    NSStackView* column=[NSStackView new];
    column.orientation=NSUserInterfaceLayoutOrientationVertical;
    column.alignment=NSLayoutAttributeLeading;
    column.spacing=8; column.translatesAutoresizingMaskIntoConstraints=NO;
    [content addSubview:column];
    [NSLayoutConstraint activateConstraints:@[
        [column.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:20],
        [column.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-20],
        [column.topAnchor constraintEqualToAnchor:content.topAnchor constant:18],
        [column.bottomAnchor constraintEqualToAnchor:content.bottomAnchor constant:-16]]];
    NSTextField* title=label(@"AtracDEnc-AAL",23);
    title.font=[NSFont boldSystemFontOfSize:23];
    [column addArrangedSubview:title];
    [column addArrangedSubview:label(text(L"简易测试文件转换器 · 基于 AtracDEnc 的独立 AAL 编解码器"))];
    NSTextField* requirements=label(text(L"编码：44,100 Hz / 16-bit / 双声道整数 PCM WAV（RIFF / RF64）。\n解码：未加密 AAL3/AALP；流式处理，无固定 1 GiB 上限。"),12);
    requirements.textColor=NSColor.secondaryLabelColor;
    [column addArrangedSubview:requirements];
    _add=button(text(L"添加音频…"),self,@selector(pickFiles:));
    _remove=button(text(L"移除选中"),self,@selector(removeFiles:));
    _clear=button(text(L"清空队列"),self,@selector(clearFiles:));
    _profile=[[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    for (const auto& p:independent_aal::kOfficialProfiles) {
        [_profile addItemWithTitle:[NSString stringWithFormat:@"%s / %s %u kbps",p.container_name,
            p.codec==independent_aal::CoreCodec::Atrac3plus?"ATRAC3plus":"ATRAC3",unsigned(p.nominal_bitrate/1000)]];
    }
    [_profile selectItemAtIndex:9];
    NSStackView* toolbar=horizontal(@[_add,_remove,_clear,label(text(L"编码核心")),_profile]);
    [column addArrangedSubview:toolbar];
    _table=[[NSTableView alloc] initWithFrame:NSZeroRect];
    _table.dataSource=self; _table.delegate=self; _table.allowsMultipleSelection=YES;
    _table.usesAlternatingRowBackgroundColors=YES; _table.rowHeight=24;
    NSArray<NSString*>* headings=@[text(L"文件名称"),text(L"状态"),text(L"进度"),text(L"输出文件")];
    const CGFloat widths[]={290,90,70,290};
    for (NSUInteger i=0;i<headings.count;++i) {
        NSTableColumn* c=[[NSTableColumn alloc] initWithIdentifier:[NSString stringWithFormat:@"%lu",(unsigned long)i]];
        c.title=headings[i]; c.width=widths[i]; c.minWidth=55; c.editable=NO;
        [_table addTableColumn:c];
    }
    NSScrollView* list=[NSScrollView new];
    list.documentView=_table; list.hasVerticalScroller=YES; list.hasHorizontalScroller=YES;
    list.borderType=NSBezelBorder;
    [list.heightAnchor constraintGreaterThanOrEqualToConstant:140].active=YES;
    [column addArrangedSubview:list];
    [column addArrangedSubview:label(text(L"输出文件夹（重名自动加序号；编码时保存校验报告）"),12)];
    _output=[[NSTextField alloc] initWithFrame:NSZeroRect];
    _output.placeholderString=text(L"默认保存到首个音源目录下的 AAL 文件夹");
    [_output.widthAnchor constraintGreaterThanOrEqualToConstant:560].active=YES;
    _browse=button(text(L"选择文件夹…"),self,@selector(pickFolder:));
    NSStackView* folder=horizontal(@[_output,_browse]); [column addArrangedSubview:folder];
    _phase=label(text(L"准备就绪：添加 PCM WAV 或 AAL OMA/AA3"),12);
    _phase.maximumNumberOfLines=1; _phase.lineBreakMode=NSLineBreakByTruncatingMiddle;
    [column addArrangedSubview:_phase];
    _progress=[NSProgressIndicator new]; _progress.indeterminate=NO;
    _progress.minValue=0; _progress.maxValue=100;
    [column addArrangedSubview:_progress];
    _start=button(text(L"开始转换"),self,@selector(convert:)); _start.keyEquivalent=@"\r";
    _cancel=button(text(L"取消"),self,@selector(cancel:));
    _selfTest=button(text(L"内置自检"),self,@selector(selfTest:));
    NSButton* open=button(text(L"打开输出文件夹"),self,@selector(openFolder:));
    _summary=label(@"",12);
    NSStackView* actions=horizontal(@[_start,_cancel,_selfTest,open,_summary]);
    [column addArrangedSubview:actions];
    [column addArrangedSubview:label(text(L"处理日志"),12)];
    NSScrollView* logs=[NSScrollView new];
    logs.hasVerticalScroller=YES; logs.borderType=NSBezelBorder;
    _log=[[NSTextView alloc] initWithFrame:NSMakeRect(0,0,820,96)];
    _log.editable=NO; _log.font=[NSFont monospacedSystemFontOfSize:11 weight:NSFontWeightRegular];
    _log.autoresizingMask=NSViewWidthSizable; _log.textContainer.widthTracksTextView=YES;
    logs.documentView=_log;
    [logs.heightAnchor constraintEqualToConstant:110].active=YES;
    [column addArrangedSubview:logs];
    NSTextField* footer=label(text(L"实验版 0.5.0 · 仅支持 High-Speed Transfer 双层 AAL；不支持 Standard。"),11);
    footer.textColor=NSColor.secondaryLabelColor; [column addArrangedSubview:footer];
    for (NSView* view in @[requirements,toolbar,list,folder,_phase,_progress,actions,logs,footer])
        [view.widthAnchor constraintEqualToAnchor:column.widthAnchor].active=YES;
    [list setContentHuggingPriority:NSLayoutPriorityDefaultLow forOrientation:NSLayoutConstraintOrientationVertical];
    [self updateControls]; [self addURLs:_startupURLs];
    [self log:text(L"WAV → AAL；AAL → WAV。仅支持已列出的 CD 规格和十档双层配置。")];
    [_window center]; [_window makeKeyAndOrderFront:nil]; [NSApp activateIgnoringOtherApps:YES];
}
- (void)log:(NSString*)message {
    NSString* line=[message stringByAppendingString:@"\n"];
    if (_log.string.length>350000) [_log.textStorage deleteCharactersInRange:NSMakeRange(0,100000)];
    [_log.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:line
        attributes:@{NSFontAttributeName:_log.font,NSForegroundColorAttributeName:NSColor.textColor}]];
    [_log scrollRangeToVisible:NSMakeRange(_log.string.length,0)];
}
- (void)updateControls {
    size_t completed=0; for (const auto& row:_rows) if (row.state==State::Done) ++completed;
    _summary.stringValue=[NSString stringWithFormat:@"%lu%@%lu",(unsigned long)_rows.size(),text(L" 个文件  ·  已完成 "),(unsigned long)completed];
    for (NSControl* c in @[_add,_remove,_clear,_profile,_output,_browse,_selfTest]) c.enabled=!_busy&&!_closing;
    _start.enabled=!_busy&&!_closing&&completed<_rows.size();
    _cancel.enabled=_busy&&!_cancelled.load();
}
- (NSInteger)numberOfRowsInTableView:(NSTableView*)table { (void)table; return (NSInteger)_rows.size(); }
- (NSView*)tableView:(NSTableView*)table viewForTableColumn:(NSTableColumn*)column row:(NSInteger)index {
    (void)table;
    const Row& row=_rows.at((size_t)index);
    NSString* value=@"";
    switch (column.identifier.integerValue) {
        case 0:value=filename(row.input);break;
        case 1: {
            const wchar_t* states[]={L"等待",L"处理中",L"已完成",L"失败",L"已取消"};
            value=text(states[(int)row.state]);break;
        }
        case 2:value=[NSString stringWithFormat:@"%d%%",row.percent];break;
        case 3:value=row.output.empty()?@"—":filename(row.output);break;
    }
    NSTextField* cell=[NSTextField labelWithString:value];
    cell.lineBreakMode=NSLineBreakByTruncatingMiddle;
    cell.toolTip=column.identifier.integerValue==0?[NSString stringWithUTF8String:row.input.c_str()]:value;
    return cell;
}
- (void)addURLs:(NSArray<NSURL*>*)urls {
    if (_busy||_closing) { [self log:text(L"当前任务结束后，可继续拖入文件。")]; return; }
    for (NSURL* url in urls) {
        if (!url.isFileURL) continue;
        fs::path path=file_path(url);
        const wchar_t* problem=ui::input_problem(path);
        if (problem) { [self log:[NSString stringWithFormat:@"%@%@ — %@",text(L"已跳过："),url.lastPathComponent,text(problem)]]; continue; }
        std::error_code ec; auto canonical=fs::weakly_canonical(path,ec); if(!ec)path=canonical;
        bool duplicate=false;
        for(const auto& row:_rows) if(row.input==path) {duplicate=true;break;}
        if(duplicate)continue;
        if (!_output.stringValue.length) _output.stringValue=[NSString stringWithUTF8String:(path.parent_path()/"AAL").c_str()];
        _rows.push_back({path,{},State::Pending,0});
    }
    [_table reloadData]; [self updateControls];
}
- (void)pickFiles:(id)sender {
    (void)sender; if(_busy)return;
    NSOpenPanel* panel=[NSOpenPanel openPanel];
    panel.canChooseDirectories=NO; panel.allowsMultipleSelection=YES;
    panel.allowedFileTypes=@[@"wav",@"oma",@"aa3"]; panel.title=text(L"添加 PCM WAV 或 AAL OMA/AA3");
    [panel beginSheetModalForWindow:_window completionHandler:^(NSModalResponse response) {
        if(response==NSModalResponseOK)[self addURLs:panel.URLs];
    }];
}
- (void)pickFolder:(id)sender {
    (void)sender; if(_busy)return;
    NSOpenPanel* panel=[NSOpenPanel openPanel];
    panel.canChooseFiles=NO; panel.canChooseDirectories=YES; panel.canCreateDirectories=YES;
    panel.title=text(L"选择转换结果的保存文件夹");
    [panel beginSheetModalForWindow:_window completionHandler:^(NSModalResponse response) {
        if(response==NSModalResponseOK)self->_output.stringValue=panel.URL.path;
    }];
}
- (void)removeFiles:(id)sender {
    (void)sender; if(_busy)return;
    NSIndexSet* indices=_table.selectedRowIndexes;
    for (size_t i=_rows.size();i>0;--i)if([indices containsIndex:i-1])_rows.erase(_rows.begin()+(i-1));
    [_table reloadData];[self updateControls];
}
- (void)clearFiles:(id)sender { (void)sender;if(!_busy){_rows.clear();[_table reloadData];[self updateControls];} }
- (void)openFolder:(id)sender {
    (void)sender; if(!_output.stringValue.length)return;
    NSURL* url=[NSURL fileURLWithPath:_output.stringValue isDirectory:YES];
    std::error_code ec;
    if(fs::is_directory(file_path(url),ec)) [[NSWorkspace sharedWorkspace] openURL:url];
    else [self log:text(L"输出文件夹尚未创建，开始转换后将自动建立。")];
}
- (void)cancel:(id)sender {
    (void)sender;if(!_busy)return;
    _cancelled.store(true);_phase.stringValue=text(L"正在取消并清理，请稍候…");[self updateControls];
}
- (void)convert:(id)sender { (void)sender;[self beginWork:NO]; }
- (void)selfTest:(id)sender { (void)sender;[self beginWork:YES]; }
- (void)beginWork:(BOOL)testOnly {
    if(_busy||_closing)return;
    if(!testOnly&&!_output.stringValue.length){[self log:text(L"请先设置输出文件夹。")];return;}
    if(_worker.joinable())_worker.join();
    std::vector<std::pair<size_t,fs::path>> jobs;
    if(!testOnly)for(size_t i=0;i<_rows.size();++i)if(_rows[i].state!=State::Done){
        jobs.emplace_back(i,_rows[i].input);_rows[i].state=State::Pending;_rows[i].percent=0;_rows[i].output.clear();
    }
    if(!testOnly&&jobs.empty())return;
    const fs::path folder=testOnly?fs::path():file_path([NSURL fileURLWithPath:_output.stringValue isDirectory:YES]);
    independent_aal::EncodeOptions options;
    options.profile_id=independent_aal::kOfficialProfiles.at((size_t)_profile.indexOfSelectedItem).id;
    _cancelled.store(false);_busy=YES;_progress.doubleValue=0;
    _phase.stringValue=text(L"正在准备转换…");[_table reloadData];[self updateControls];
    AALController* controller=self;
    try {
        _worker=std::thread([controller,jobs,folder,options,testOnly] {
            @autoreleasepool {
                unsigned passed=0,failed=0;
                try {
                    if(!testOnly)fs::create_directories(folder);
                    auto progressFor=[controller](size_t index) {
                        return [controller,index,last=-1,lastStage=std::wstring()](int percent,const std::wstring& stage) mutable {
                            if(percent==last&&stage==lastStage)return;
                            last=percent;lastStage=stage;
                            NSString* message=ns(ui::progress_text(stage));
                            dispatch_async(dispatch_get_main_queue(),^{
                                if(index<controller->_rows.size()) {controller->_rows[index].percent=percent;[controller->_table reloadData];}
                                controller->_progress.doubleValue=percent;
                                if(!controller->_cancelled.load())controller->_phase.stringValue=message;
                            });
                        };
                    };
                    if(testOnly) {
                        independent_aal::self_test(progressFor(size_t(-1)),&controller->_cancelled,options);++passed;
                    } else for(const auto& job:jobs) {
                        if(controller->_cancelled.load())break;
                        const size_t index=job.first;
                        const fs::path input=job.second;
                        try {
                            const fs::path output=ui::vacant_output(folder,job.second);
                            dispatch_async(dispatch_get_main_queue(),^{
                                controller->_rows[index].state=State::Working;controller->_rows[index].output=output;
                                [controller->_table reloadData];
                            });
                            std::string hash;
                            if(ui::is_decode_input(job.second))hash=independent_aal::decode_file(job.second,output,progressFor(index),&controller->_cancelled).pcm_sha256;
                            else hash=independent_aal::encode_file(job.second,output,progressFor(index),&controller->_cancelled,options).pcm_sha256;
                            NSString* checksum=[NSString stringWithUTF8String:hash.c_str()];
                            dispatch_async(dispatch_get_main_queue(),^{
                                controller->_rows[index].state=State::Done;controller->_rows[index].percent=100;
                                [controller->_table reloadData];
                                [controller log:[NSString stringWithFormat:@"%@%@ → %@\nPCM SHA-256: %@",text(L"完成："),filename(input),filename(output),checksum]];
                            });++passed;
                        } catch(const std::exception& error) {
                            const bool cancelled=controller->_cancelled.load()||std::string(error.what())=="Cancelled";
                            if(cancelled)controller->_cancelled.store(true);else ++failed;
                            NSString* reason=[NSString stringWithUTF8String:error.what()]?:@"Error";
                            dispatch_async(dispatch_get_main_queue(),^{
                                controller->_rows[index].state=cancelled?State::Cancelled:State::Failed;
                                [controller->_table reloadData];
                                [controller log:[NSString stringWithFormat:@"%@%@ — %@",text(cancelled?L"已取消：":L"转换失败："),filename(input),reason]];
                            });if(cancelled)break;
                        }
                    }
                } catch(const std::exception& error) {
                    if(controller->_cancelled.load()||std::string(error.what())=="Cancelled")controller->_cancelled.store(true);
                    else {++failed;NSString* reason=[NSString stringWithUTF8String:error.what()]?:@"Error";
                        dispatch_async(dispatch_get_main_queue(),^{[controller log:reason];});}
                } catch(...) {++failed;dispatch_async(dispatch_get_main_queue(),^{[controller log:text(L"处理失败，请检查音频文件和输出目录后重试。")];});}
                dispatch_async(dispatch_get_main_queue(),^{
                    if(controller->_worker.joinable())controller->_worker.join();
                    controller->_busy=NO;
                    NSString* message=controller->_cancelled.load()?text(L"已取消。已完成的文件已保留，可再次开始处理其余文件。"):
                        testOnly?(failed?text(L"自检未通过，请查看日志。"):text(L"内置自检通过。")):
                        [NSString stringWithFormat:@"%@%u%@%u%@",text(L"本次处理结束：成功 "),passed,text(L" 个，失败 "),failed,text(L" 个。")];
                    controller->_phase.stringValue=message;[controller log:message];[controller updateControls];
                    if(controller->_terminating)[NSApp replyToApplicationShouldTerminate:YES];
                    else if(controller->_closing)[controller->_window close];
                });
            }
        });
    }catch(const std::exception& error){_busy=NO;[self updateControls];[self log:[NSString stringWithUTF8String:error.what()]];}
}
- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;if(!_busy)return YES;
    _closing=YES;[self cancel:nil];[self updateControls];return NO;
}
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    (void)sender;if(!_busy)return NSTerminateNow;
    _terminating=YES;_closing=YES;[self cancel:nil];[self updateControls];return NSTerminateLater;
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender { (void)sender;return YES; }
- (void)application:(NSApplication*)sender openURLs:(NSArray<NSURL*>*)urls {
    (void)sender;
    if(_table)[self addURLs:urls];
    else _startupURLs=[_startupURLs arrayByAddingObjectsFromArray:urls];
}
@end

int main(int argc,const char* argv[]) {
    @autoreleasepool {
        std::vector<std::wstring> tags,args;
        for(NSString* tag in NSLocale.preferredLanguages)tags.push_back(aal_wide(tag));
        bool headless=false;
        for(int i=1;i<argc;++i) {
            if(!std::strcmp(argv[i],"--self-test")){headless=true;continue;}
            if(!std::strncmp(argv[i],"-psn_",5))continue;
            args.push_back(aal_wide([NSString stringWithUTF8String:argv[i]]?:@""));
        }
        ui::Startup startup;
        if(!ui::parse_arguments(args,ui::preferred_language(tags),startup)) {
            std::cerr<<"Invalid arguments. Use --lang auto|zh|en and optional input files.\n";return 4;
        }
        ui::language=startup.language;
        if(headless) {
            try {independent_aal::self_test();std::cout<<"PASS: native AppKit converter entry point and linked codec; no window launched\n";return 0;}
            catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
        }
        NSMutableArray<NSURL*>* urls=[NSMutableArray new];
        for(const auto& path:startup.paths)[urls addObject:[NSURL fileURLWithPath:ns(path)]];
        [NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        AALController* controller=[[AALController alloc] initWithURLs:urls];
        NSApp.delegate=controller;[NSApp run];
    }
    return 0;
}
